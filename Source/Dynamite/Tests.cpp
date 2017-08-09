//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#define _CRT_SECURE_NO_WARNINGS // "secure" CRT not available on all platforms  --add this at the top of all CPP files that give "function or variable may be unsafe" warnings

#include "CNTKLibrary.h"
#include "CNTKLibraryHelpers.h"
#include "PlainTextDeseralizer.h"
#include "Layers.h"
#include "TimerUtility.h"
//#include "../Math/CommonMatrix.h"
// TODO: pull this from a header
enum ElementWiseOperator
{
    // nullary
    opConstOne, opNone,
    // unary (or binary with constant parameter)
    opCopy,
    opNegate, opNot, opAbs, opFloor, opReciprocal,
    opSigmoid, opTanh, opSqr, opSqrt, opExp, opLog, opLinearRectifier, opCosine, opSin, opExponentialLinearUnit, opStableSigmoid,
    // unary ops for use by Matrix class only (there is no TensorView implementation)
    opSigmoidDerivative, opLinearRectifierDerivative, opNegativeSine, opExponentialLinearUnitDerivative, opStableSigmoidDerivative,
    // binary
    opCopyIf, opCopyIfNot, opSum, opDifference, opElementwiseProduct, opElementwiseQuotient, opLogSum, opPow,
    opMax, opMin, opArgmax, opArgmin,
    opLess, opEqual, opGreater, opGreaterEqual, opNotEqual, opLessEqual, // Note: must obey this order: (sgn(a-b) == -1, 0, +1), (sgn(a-b) != -1, 0, +1)
    opAnd, opOr, opXor, opMaskNegative,
    opElementwiseProductWithSigmoidDerivativeFromOutput, opElementwiseProductWithTanhDerivativeFromOutput,
    opElementwiseProductWithLinearRectifierDerivativeFromOutput, opElementwiseProductWithLogDerivativeFromOutput,
    opElementwiseProductWithCosDerivative, opElementwiseProductWithSinDerivative,
    opElementwiseProductWithAbsDerivative, opElementwiseProductWithSqrtDerivative,
    opElementwiseProductWithReciprocalDerivative, opSqrOfDifference,
    opElementwiseProductWithExponentialLinearUnitDerivativeFromOutput,
    // binary ops for indexing
    // opIndex,
    // ternary
    opCond /*a ? b : c*/,
    opClip, /*clip a within interval b..c*/
    opElementwiseProductWithLogSumDerivative,
    opCopyIfEqual,
    opElementwiseProductWithExpOfDiff, /* a * exp(b - c) */
    opElementwiseProductWithQuotient, /* a * (b / c) */
    opElementwiseProductWithPowExponentDerivative, /* a * b * log(c) */
    opElementwiseProductWithPowBaseDerivative,  /* a * c * pow(b, c-1) */
                                                // Note: not all that's implemented in CNTK ComputationNodes has an opcode yet.
};


#include <cstdio>
#include <map>
#include <set>
#include <vector>

#define let const auto

using namespace CNTK;
using namespace std;

using namespace Dynamite;

#define Op(opCode) (pair<function<NDArrayViewPtr(const vector<NDArrayViewPtr>&)>, const char*>([=](const vector<NDArrayViewPtr>& argValues){ return NDArrayView::NumericOperation(argValues, 1.0, op##opCode); }, #opCode))

struct TensorViewTest
{
    pair<function<NDArrayViewPtr(const vector<NDArrayViewPtr>&)>, const char*> op;
    function<Variable(const vector<Variable>& args)> f;
    vector<NDShape> shapes;
};

size_t DynamiteTest(size_t N, DataType dataType, const DeviceDescriptor& device)
{
    size_t numFailed = 0;
    unsigned long seed = 1;
    // for testing batching of the matrix product, we need a shared matrix
    let sharedMatrix = (dataType == DataType::Float)
                        ? NDArrayView::RandomNormal<float> (NDShape{ 13, 42 }, /*mean=*/0., /*stdDev=*/0.3, seed++, device)
                        : NDArrayView::RandomNormal<double>(NDShape{ 13, 42 }, /*mean=*/0., /*stdDev=*/0.3, seed++, device);
    let sharedMatrixVar = Constant(sharedMatrix);
    vector<TensorViewTest> tests =
    {
        // matrix product
        { { [&](const vector<NDArrayViewPtr>& argValues) { return NDArrayView::MatrixProduct(false, sharedMatrix, false, argValues[1], false, 1.0, 1); }, "Times"          }, [&](const vector<Variable>& args) { return CNTK::Times (sharedMatrixVar, args[1]); },{ { 13, 42 },{ 42, 9 } } },
        { { [&](const vector<NDArrayViewPtr>& argValues) { return NDArrayView::MatrixProduct(false, argValues[0], false, argValues[1], false, 1.0, 1); }, "Times"          }, [&](const vector<Variable>& args) { return CNTK::Times         (args[0], args[1]); },{ { 13, 42 },{ 42, 9 } } },
        { { [&](const vector<NDArrayViewPtr>& argValues) { return NDArrayView::MatrixProduct(false, argValues[0], false, argValues[1], false, 1.0, 1); }, "Times"          }, [&](const vector<Variable>& args) { return CNTK::Times         (args[0], args[1]); },{ { 13, 42 },{ 42 } } },
        { { [&](const vector<NDArrayViewPtr>& argValues) { return NDArrayView::MatrixProduct(false, argValues[0], false, argValues[1], false, 1.0, 1); }, "Times"          }, [&](const vector<Variable>& args) { return CNTK::Times         (args[0], args[1]); },{ { 13, 42 },{ 42, 9, 5 } } },
        { { [&](const vector<NDArrayViewPtr>& argValues) { return NDArrayView::MatrixProduct(false, argValues[0], true,  argValues[1], false, 1.0, 1); }, "TransposeTimes" }, [&](const vector<Variable>& args) { return CNTK::TransposeTimes(args[0], args[1]); },{ { 42, 13 },{ 42, 9 } } },
        { { [&](const vector<NDArrayViewPtr>& argValues) { return NDArrayView::MatrixProduct(false, argValues[0], true,  argValues[1], false, 1.0, 1); }, "TransposeTimes" }, [&](const vector<Variable>& args) { return CNTK::TransposeTimes(args[0], args[1]); },{ { 42, 13 },{ 42 } } },
        { { [&](const vector<NDArrayViewPtr>& argValues) { return NDArrayView::MatrixProduct(false, argValues[0], true,  argValues[1], false, 1.0, 1); }, "TransposeTimes" }, [&](const vector<Variable>& args) { return CNTK::TransposeTimes(args[0], args[1]); },{ { 42, 13 },{ 42, 9, 3 } } },
        // ternary
        { Op(Clip                 ), [](const vector<Variable>& args) { return CNTK::Clip         (args[0], args[1], args[2]); }, { { 13, 42 }, { 13, 1 }, { 13, 1 } } },
        { Op(Cond                 ), [](const vector<Variable>& args) { return CNTK::ElementSelect(args[0], args[1], args[2]); }, { { 13, 42 }, { 13, 1 }, { 13, 1 } } },
        // binary
        { Op(Sum                  ), [](const vector<Variable>& args) { return CNTK::Plus         (args[0], args[1]); }, { { 13, 42 }, { 13, 42 } } },
        { Op(Difference           ), [](const vector<Variable>& args) { return CNTK::Minus        (args[0], args[1]); }, { { 13, 42 }, { 13, 1 } } },
        { Op(ElementwiseProduct   ), [](const vector<Variable>& args) { return CNTK::ElementTimes (args[0], args[1]); }, { { 13, 42 }, { 13, 1 } } },
        { Op(LogSum               ), [](const vector<Variable>& args) { return CNTK::LogAddExp    (args[0], args[1]); }, { { 13, 42 }, { 13, 1 } } },
        { Op(Pow                  ), [](const vector<Variable>& args) { return CNTK::Pow          (args[0], args[1]); }, { { 13, 42, 12 }, { 13, 1 } } },
        { Op(Equal                ), [](const vector<Variable>& args) { return CNTK::Equal        (args[0], args[1]); }, { { 13, 42 }, { 13, 1 } } },
        { Op(NotEqual             ), [](const vector<Variable>& args) { return CNTK::NotEqual     (args[0], args[1]); }, { { 13, 42 }, { 13, 42 } } },
        { Op(Less                 ), [](const vector<Variable>& args) { return CNTK::Less         (args[0], args[1]); }, { { 13, 42 }, { 13, 1 } } },
        { Op(LessEqual            ), [](const vector<Variable>& args) { return CNTK::LessEqual    (args[0], args[1]); }, { { 13, 42 }, { 13, 1 } } },
        { Op(Greater              ), [](const vector<Variable>& args) { return CNTK::Greater      (args[0], args[1]); }, { { 13, 42 }, { 13, 1 } } },
        { Op(GreaterEqual         ), [](const vector<Variable>& args) { return CNTK::GreaterEqual (args[0], args[1]); }, { { 13, 42 }, { 13, 1 } } },
        // unary
        { Op(LinearRectifier      ), [](const vector<Variable>& args) { return CNTK::ReLU         (args[0]         ); }, { { 13, 42 } } },
        { Op(Tanh                 ), [](const vector<Variable>& args) { return CNTK::Tanh         (args[0]         ); }, { { 13 } } },
        { Op(Log                  ), [](const vector<Variable>& args) { return CNTK::Log          (args[0]         ); }, { { 13, 42 } } },
        { Op(Exp                  ), [](const vector<Variable>& args) { return CNTK::Exp          (args[0]         ); }, { { 13, 42 } } },
        { Op(Cosine               ), [](const vector<Variable>& args) { return CNTK::Cos          (args[0]         ); }, { { 13, 42 } } },
        { Op(Sin                  ), [](const vector<Variable>& args) { return CNTK::Sin          (args[0]         ); }, { { 235, 13, 2 } } },
        { Op(Negate               ), [](const vector<Variable>& args) { return CNTK::Negate       (args[0]         ); }, { { 13 } } },
        { Op(Floor                ), [](const vector<Variable>& args) { return CNTK::Floor        (args[0]         ); }, { { 13, 42 } } },
        { Op(Abs                  ), [](const vector<Variable>& args) { return CNTK::Abs          (args[0]         ); }, { { 13, 42 } } },
        { Op(Sqrt                 ), [](const vector<Variable>& args) { return CNTK::Sqrt         (args[0]         ); }, { { 13, 42 } } },
        { Op(Reciprocal           ), [](const vector<Variable>& args) { return CNTK::Reciprocal   (args[0]         ); }, { { 13, 42 } } },
        { Op(ExponentialLinearUnit), [](const vector<Variable>& args) { return CNTK::ELU          (args[0]         ); }, { { 13, 42 } } },
        { Op(StableSigmoid        ), [](const vector<Variable>& args) { return CNTK::Sigmoid      (args[0]         ); }, { { 128 } } }
    };

    fprintf(stderr, "\n--- batch of %d. %s on %S\n\n", (int)N, CNTK::DataTypeName(dataType), device.AsString().c_str());
    for (let& test : tests)
    {
        NDArrayViewPtr refVal;
        Variable resVar;
        for (size_t n = 0; n < N; n++)
        {
            vector<NDArrayViewPtr> argValues;
            for (let& shape : test.shapes)
                if (dataType == DataType::Float)
                    argValues.push_back(NDArrayView::RandomNormal<float> (shape, /*mean=*/0., /*stdDev=*/0.3, seed++, device));
                else
                    argValues.push_back(NDArrayView::RandomNormal<double>(shape, /*mean=*/0., /*stdDev=*/0.3, seed++, device));
            // reference: TensorView op directly
            let refVal1 = test.op.first(argValues);
            if (n > 0)
                refVal = NDArrayView::NumericOperation({ refVal, refVal1 }, 1.0, ElementWiseOperator::opSum);
            else
                refVal = refVal1;
#if 0
            for (let& arg : argValues)
                arg->LogToFile(L"argVal", stderr);
            refVal1->LogToFile(L"resVal", stderr);
            refVal->LogToFile(L"sumVal", stderr);
#endif
            // Dynamite:
            vector<Variable> args;
            for (let& argValue : argValues)
                args.push_back(Constant(argValue));
            if (n == 0)
            {
                fprintf(stderr, "%25s(", test.op.second);
                for (let& arg : args)
                    fprintf(stderr, " %S ", arg.Shape().AsString().c_str());
            }
            Variable resVar1 = test.f(args);
            if (n > 0)
                resVar = CNTK::Plus(resVar, resVar1);
            else
                resVar = resVar1;
        }
        let resVal = resVar.Value();
        fprintf(stderr, ") -> %S\n", resVal->AsString().c_str());
        let sqrErr = NDArrayView::NumericOperation({ resVal, refVal }, 1.0 / refVal->Shape().TotalSize(), ElementWiseOperator::opSqrOfDifference, make_shared<NDArrayView>(dataType, NDShape{}, device), 0, ElementWiseOperator::opSum);
        let avSqrErr = sqrErr->AsScalar<double>();
        if (avSqrErr > 1e-5)
        {
            fprintf(stderr, "################# FAILED: avSqrErr = %.2f\n", avSqrErr);
            numFailed++;
        }
    }
    return numFailed;
}

void RunDynamiteTests()
{
    size_t numFailed = 0;
    numFailed += DynamiteTest(3, DataType::Double, DeviceDescriptor::CPUDevice());
    numFailed += DynamiteTest(3, DataType::Float, DeviceDescriptor::GPUDevice(0));
    numFailed += DynamiteTest(1, DataType::Double, DeviceDescriptor::CPUDevice());
    numFailed += DynamiteTest(1, DataType::Double, DeviceDescriptor::GPUDevice(0));
    numFailed += DynamiteTest(1, DataType::Float, DeviceDescriptor::GPUDevice(0));
    numFailed += DynamiteTest(1, DataType::Float, DeviceDescriptor::CPUDevice());
    if (numFailed > 0)
        LogicError("RunDynamiteTests: %d tests failed.", (int)numFailed);
    exit(0);
}
