// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/dependency_parser_op_resolver.h"

#include "components/optimization_guide/core/optimization_guide_features.h"
#include "third_party/tensorflow_models/src/research/seq_flow_lite/tflite_ops/sequence_string_projection.h"
#include "third_party/tensorflow_models/src/research/seq_flow_lite/tflite_ops/tflite_qrnn_pooling.h"
#include "third_party/tflite/buildflags.h"
#include "third_party/tflite/src/tensorflow/lite/c/common.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/builtin_op_kernels.h"

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
#include "third_party/tflite/src/tensorflow/lite/tflite_with_xnnpack_optional.h"
#endif

DependencyParserOpResolver::DependencyParserOpResolver() {
  AddBuiltin(tflite::BuiltinOperator_CONCATENATION,
             tflite::ops::builtin::Register_CONCATENATION());
  AddBuiltin(tflite::BuiltinOperator_CONV_2D,
             tflite::ops::builtin::Register_CONV_2D());
  AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
             tflite::ops::builtin::Register_FULLY_CONNECTED());
  AddBuiltin(tflite::BuiltinOperator_LOG_SOFTMAX,
             tflite::ops::builtin::Register_LOG_SOFTMAX());
  AddBuiltin(tflite::BuiltinOperator_LOGISTIC,
             tflite::ops::builtin::Register_LOGISTIC());
  AddBuiltin(tflite::BuiltinOperator_MUL, tflite::ops::builtin::Register_MUL());
  AddBuiltin(tflite::BuiltinOperator_PAD, tflite::ops::builtin::Register_PAD());
  AddBuiltin(tflite::BuiltinOperator_RESHAPE,
             tflite::ops::builtin::Register_RESHAPE());
  AddBuiltin(tflite::BuiltinOperator_SHAPE,
             tflite::ops::builtin::Register_SHAPE());
  AddBuiltin(tflite::BuiltinOperator_STRIDED_SLICE,
             tflite::ops::builtin::Register_STRIDED_SLICE());
  AddBuiltin(tflite::BuiltinOperator_SUB, tflite::ops::builtin::Register_SUB());
  AddBuiltin(tflite::BuiltinOperator_TANH,
             tflite::ops::builtin::Register_TANH());
  AddCustom("PoolingOp", seq_flow_lite::ops::custom::Register_QRNN_POOLING());
  AddCustom(
      "SequenceStringProjectionV2",
      seq_flow_lite::ops::custom::Register_SEQUENCE_STRING_PROJECTION_V2());

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  if (optimization_guide::features::TFLiteXNNPACKDelegateEnabled()) {
    delegate_creators_.push_back([](TfLiteContext* context) {
      return tflite::MaybeCreateXNNPACKDelegate(
          context, tflite::XNNPackQS8Options::default_value);
    });
  }
#endif
}
DependencyParserOpResolver::~DependencyParserOpResolver() = default;
