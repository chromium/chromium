// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/optimization_guide/core/tflite_op_resolver.h"

#include "components/optimization_guide/core/optimization_guide_features.h"
#include "third_party/tflite/buildflags.h"
#include "third_party/tflite/src/tensorflow/lite/c/common.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/builtin_op_kernels.h"
#include "third_party/tflite/src/tensorflow/lite/schema/schema_generated.h"

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
#include "third_party/tflite/src/tensorflow/lite/tflite_with_xnnpack_optional.h"
#endif

namespace optimization_guide {

TFLiteOpResolver::TFLiteOpResolver() {
  AddBuiltin(tflite::BuiltinOperator_ABS, tflite::ops::builtin::Register_ABS());
  AddBuiltin(tflite::BuiltinOperator_HARD_SWISH,
             tflite::ops::builtin::Register_HARD_SWISH());
  AddBuiltin(tflite::BuiltinOperator_RELU,
             tflite::ops::builtin::Register_RELU(), /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_RELU_N1_TO_1,
             tflite::ops::builtin::Register_RELU_N1_TO_1());
  AddBuiltin(tflite::BuiltinOperator_RELU6,
             tflite::ops::builtin::Register_RELU6(), /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_TANH,
             tflite::ops::builtin::Register_TANH(), /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_LOGISTIC,
             tflite::ops::builtin::Register_LOGISTIC(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_AVERAGE_POOL_2D,
             tflite::ops::builtin::Register_AVERAGE_POOL_2D(),
             /* min_version */ 1,
             /* max_version */ 3);
  AddBuiltin(tflite::BuiltinOperator_MAX_POOL_2D,
             tflite::ops::builtin::Register_MAX_POOL_2D(),
             /* min_version */ 1,
             /* max_version */ 3);
  AddBuiltin(tflite::BuiltinOperator_L2_POOL_2D,
             tflite::ops::builtin::Register_L2_POOL_2D());
  AddBuiltin(tflite::BuiltinOperator_CONV_2D,
             tflite::ops::builtin::Register_CONV_2D(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
             tflite::ops::builtin::Register_DEPTHWISE_CONV_2D(),
             /* min_version = */ 1,
             /* max_version = */ 5);
  AddBuiltin(tflite::BuiltinOperator_SVDF,
             tflite::ops::builtin::Register_SVDF(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_RNN, tflite::ops::builtin::Register_RNN(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_BIDIRECTIONAL_SEQUENCE_RNN,
             tflite::ops::builtin::Register_BIDIRECTIONAL_SEQUENCE_RNN(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_UNIDIRECTIONAL_SEQUENCE_RNN,
             tflite::ops::builtin::Register_UNIDIRECTIONAL_SEQUENCE_RNN(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_EMBEDDING_LOOKUP,
             tflite::ops::builtin::Register_EMBEDDING_LOOKUP(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_EMBEDDING_LOOKUP_SPARSE,
             tflite::ops::builtin::Register_EMBEDDING_LOOKUP_SPARSE());
  AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
             tflite::ops::builtin::Register_FULLY_CONNECTED(),
             /* min_version = */ 1,
             /* max_version = */ 12);
  AddBuiltin(tflite::BuiltinOperator_LSH_PROJECTION,
             tflite::ops::builtin::Register_LSH_PROJECTION());
  AddBuiltin(tflite::BuiltinOperator_HASHTABLE_LOOKUP,
             tflite::ops::builtin::Register_HASHTABLE_LOOKUP());
  AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
             tflite::ops::builtin::Register_SOFTMAX(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_CONCATENATION,
             tflite::ops::builtin::Register_CONCATENATION(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_ADD, tflite::ops::builtin::Register_ADD(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_SPACE_TO_BATCH_ND,
             tflite::ops::builtin::Register_SPACE_TO_BATCH_ND(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_BATCH_TO_SPACE_ND,
             tflite::ops::builtin::Register_BATCH_TO_SPACE_ND(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_MUL, tflite::ops::builtin::Register_MUL(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(tflite::BuiltinOperator_L2_NORMALIZATION,
             tflite::ops::builtin::Register_L2_NORMALIZATION(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_LOCAL_RESPONSE_NORMALIZATION,
             tflite::ops::builtin::Register_LOCAL_RESPONSE_NORMALIZATION());
  AddBuiltin(tflite::BuiltinOperator_LSTM,
             tflite::ops::builtin::Register_LSTM(), /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_BIDIRECTIONAL_SEQUENCE_LSTM,
             tflite::ops::builtin::Register_BIDIRECTIONAL_SEQUENCE_LSTM(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_UNIDIRECTIONAL_SEQUENCE_LSTM,
             tflite::ops::builtin::Register_UNIDIRECTIONAL_SEQUENCE_LSTM(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_PAD, tflite::ops::builtin::Register_PAD(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_PADV2,
             tflite::ops::builtin::Register_PADV2(), /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_RESHAPE,
             tflite::ops::builtin::Register_RESHAPE());
  AddBuiltin(tflite::BuiltinOperator_RESIZE_BILINEAR,
             tflite::ops::builtin::Register_RESIZE_BILINEAR(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_RESIZE_NEAREST_NEIGHBOR,
             tflite::ops::builtin::Register_RESIZE_NEAREST_NEIGHBOR(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_SKIP_GRAM,
             tflite::ops::builtin::Register_SKIP_GRAM());
  AddBuiltin(tflite::BuiltinOperator_SPACE_TO_DEPTH,
             tflite::ops::builtin::Register_SPACE_TO_DEPTH(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_DEPTH_TO_SPACE,
             tflite::ops::builtin::Register_DEPTH_TO_SPACE());
  AddBuiltin(tflite::BuiltinOperator_GATHER,
             tflite::ops::builtin::Register_GATHER(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_TRANSPOSE,
             tflite::ops::builtin::Register_TRANSPOSE(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(tflite::BuiltinOperator_MEAN,
             tflite::ops::builtin::Register_MEAN(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_DIV, tflite::ops::builtin::Register_DIV(),
             /* min_version */ 1,
             /* max_version */ 2);
  AddBuiltin(tflite::BuiltinOperator_SUB, tflite::ops::builtin::Register_SUB(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_SPLIT,
             tflite::ops::builtin::Register_SPLIT(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(tflite::BuiltinOperator_SPLIT_V,
             tflite::ops::builtin::Register_SPLIT_V(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_SQUEEZE,
             tflite::ops::builtin::Register_SQUEEZE());
  AddBuiltin(tflite::BuiltinOperator_STRIDED_SLICE,
             tflite::ops::builtin::Register_STRIDED_SLICE(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(tflite::BuiltinOperator_EXP, tflite::ops::builtin::Register_EXP());
  AddBuiltin(tflite::BuiltinOperator_TOPK_V2,
             tflite::ops::builtin::Register_TOPK_V2(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_LOG, tflite::ops::builtin::Register_LOG());
  AddBuiltin(tflite::BuiltinOperator_LOG_SOFTMAX,
             tflite::ops::builtin::Register_LOG_SOFTMAX(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_CAST,
             tflite::ops::builtin::Register_CAST());
  AddBuiltin(tflite::BuiltinOperator_DEQUANTIZE,
             tflite::ops::builtin::Register_DEQUANTIZE(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(tflite::BuiltinOperator_PRELU,
             tflite::ops::builtin::Register_PRELU());
  AddBuiltin(tflite::BuiltinOperator_MAXIMUM,
             tflite::ops::builtin::Register_MAXIMUM(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(tflite::BuiltinOperator_MINIMUM,
             tflite::ops::builtin::Register_MINIMUM(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(tflite::BuiltinOperator_ARG_MAX,
             tflite::ops::builtin::Register_ARG_MAX(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_ARG_MIN,
             tflite::ops::builtin::Register_ARG_MIN(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_GREATER,
             tflite::ops::builtin::Register_GREATER(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_GREATER_EQUAL,
             tflite::ops::builtin::Register_GREATER_EQUAL(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_LESS,
             tflite::ops::builtin::Register_LESS(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_LESS_EQUAL,
             tflite::ops::builtin::Register_LESS_EQUAL(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_FLOOR,
             tflite::ops::builtin::Register_FLOOR());
  AddBuiltin(tflite::BuiltinOperator_CEIL,
             tflite::ops::builtin::Register_CEIL());
  AddBuiltin(tflite::BuiltinOperator_ROUND,
             tflite::ops::builtin::Register_ROUND());
  AddBuiltin(tflite::BuiltinOperator_NEG, tflite::ops::builtin::Register_NEG());
  AddBuiltin(tflite::BuiltinOperator_SELECT,
             tflite::ops::builtin::Register_SELECT(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_SELECT_V2,
             tflite::ops::builtin::Register_SELECT_V2());
  AddBuiltin(tflite::BuiltinOperator_SLICE,
             tflite::ops::builtin::Register_SLICE(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_SIN, tflite::ops::builtin::Register_SIN());
  AddBuiltin(tflite::BuiltinOperator_COS, tflite::ops::builtin::Register_COS());
  AddBuiltin(tflite::BuiltinOperator_TRANSPOSE_CONV,
             tflite::ops::builtin::Register_TRANSPOSE_CONV(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_TILE,
             tflite::ops::builtin::Register_TILE(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_SUM, tflite::ops::builtin::Register_SUM(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_REDUCE_PROD,
             tflite::ops::builtin::Register_REDUCE_PROD());
  AddBuiltin(tflite::BuiltinOperator_REDUCE_MAX,
             tflite::ops::builtin::Register_REDUCE_MAX(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_REDUCE_MIN,
             tflite::ops::builtin::Register_REDUCE_MIN(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_REDUCE_ANY,
             tflite::ops::builtin::Register_REDUCE_ANY());
  AddBuiltin(tflite::BuiltinOperator_EXPAND_DIMS,
             tflite::ops::builtin::Register_EXPAND_DIMS());
  AddBuiltin(tflite::BuiltinOperator_SPARSE_TO_DENSE,
             tflite::ops::builtin::Register_SPARSE_TO_DENSE(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_EQUAL,
             tflite::ops::builtin::Register_EQUAL(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_NOT_EQUAL,
             tflite::ops::builtin::Register_NOT_EQUAL(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_SQRT,
             tflite::ops::builtin::Register_SQRT());
  AddBuiltin(tflite::BuiltinOperator_RSQRT,
             tflite::ops::builtin::Register_RSQRT());
  AddBuiltin(tflite::BuiltinOperator_SHAPE,
             tflite::ops::builtin::Register_SHAPE());
  AddBuiltin(tflite::BuiltinOperator_RANK,
             tflite::ops::builtin::Register_RANK());
  AddBuiltin(tflite::BuiltinOperator_POW, tflite::ops::builtin::Register_POW());
  AddBuiltin(tflite::BuiltinOperator_FAKE_QUANT,
             tflite::ops::builtin::Register_FAKE_QUANT(), 1, 2);
  AddBuiltin(tflite::BuiltinOperator_PACK,
             tflite::ops::builtin::Register_PACK(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(tflite::BuiltinOperator_ONE_HOT,
             tflite::ops::builtin::Register_ONE_HOT());
  AddBuiltin(tflite::BuiltinOperator_LOGICAL_OR,
             tflite::ops::builtin::Register_LOGICAL_OR());
  AddBuiltin(tflite::BuiltinOperator_LOGICAL_AND,
             tflite::ops::builtin::Register_LOGICAL_AND());
  AddBuiltin(tflite::BuiltinOperator_LOGICAL_NOT,
             tflite::ops::builtin::Register_LOGICAL_NOT());
  AddBuiltin(tflite::BuiltinOperator_UNPACK,
             tflite::ops::builtin::Register_UNPACK(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(tflite::BuiltinOperator_FLOOR_DIV,
             tflite::ops::builtin::Register_FLOOR_DIV(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_SQUARE,
             tflite::ops::builtin::Register_SQUARE());
  AddBuiltin(tflite::BuiltinOperator_ZEROS_LIKE,
             tflite::ops::builtin::Register_ZEROS_LIKE());
  AddBuiltin(tflite::BuiltinOperator_FLOOR_MOD,
             tflite::ops::builtin::Register_FLOOR_MOD());
  AddBuiltin(tflite::BuiltinOperator_RANGE,
             tflite::ops::builtin::Register_RANGE());
  AddBuiltin(tflite::BuiltinOperator_LEAKY_RELU,
             tflite::ops::builtin::Register_LEAKY_RELU(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_SQUARED_DIFFERENCE,
             tflite::ops::builtin::Register_SQUARED_DIFFERENCE());
  AddBuiltin(tflite::BuiltinOperator_FILL,
             tflite::ops::builtin::Register_FILL(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_MIRROR_PAD,
             tflite::ops::builtin::Register_MIRROR_PAD(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_UNIQUE,
             tflite::ops::builtin::Register_UNIQUE());
  AddBuiltin(tflite::BuiltinOperator_REVERSE_V2,
             tflite::ops::builtin::Register_REVERSE_V2(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_ADD_N,
             tflite::ops::builtin::Register_ADD_N());
  AddBuiltin(tflite::BuiltinOperator_GATHER_ND,
             tflite::ops::builtin::Register_GATHER_ND(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_WHERE,
             tflite::ops::builtin::Register_WHERE());
  AddBuiltin(tflite::BuiltinOperator_ELU, tflite::ops::builtin::Register_ELU());
  AddBuiltin(tflite::BuiltinOperator_REVERSE_SEQUENCE,
             tflite::ops::builtin::Register_REVERSE_SEQUENCE());
  AddBuiltin(tflite::BuiltinOperator_MATRIX_DIAG,
             tflite::ops::builtin::Register_MATRIX_DIAG());
  AddBuiltin(tflite::BuiltinOperator_QUANTIZE,
             tflite::ops::builtin::Register_QUANTIZE(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_MATRIX_SET_DIAG,
             tflite::ops::builtin::Register_MATRIX_SET_DIAG());
  AddBuiltin(tflite::BuiltinOperator_IF, tflite::ops::builtin::Register_IF());
  AddBuiltin(tflite::BuiltinOperator_WHILE,
             tflite::ops::builtin::Register_WHILE());
  AddBuiltin(tflite::BuiltinOperator_NON_MAX_SUPPRESSION_V4,
             tflite::ops::builtin::Register_NON_MAX_SUPPRESSION_V4());
  AddBuiltin(tflite::BuiltinOperator_NON_MAX_SUPPRESSION_V5,
             tflite::ops::builtin::Register_NON_MAX_SUPPRESSION_V5());
  AddBuiltin(tflite::BuiltinOperator_SCATTER_ND,
             tflite::ops::builtin::Register_SCATTER_ND());
  AddBuiltin(tflite::BuiltinOperator_DENSIFY,
             tflite::ops::builtin::Register_DENSIFY());
  AddBuiltin(tflite::BuiltinOperator_SEGMENT_SUM,
             tflite::ops::builtin::Register_SEGMENT_SUM());
  AddBuiltin(tflite::BuiltinOperator_BATCH_MATMUL,
             tflite::ops::builtin::Register_BATCH_MATMUL(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(tflite::BuiltinOperator_GELU,
             tflite::ops::builtin::Register_GELU(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(tflite::BuiltinOperator_RANDOM_STANDARD_NORMAL,
             tflite::ops::builtin::Register_RANDOM_STANDARD_NORMAL());
  AddBuiltin(tflite::BuiltinOperator_RANDOM_UNIFORM,
             tflite::ops::builtin::Register_RANDOM_UNIFORM());

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  if (features::TFLiteXNNPACKDelegateEnabled()) {
    delegate_creators_.push_back([](TfLiteContext* context) {
      return tflite::MaybeCreateXNNPACKDelegate(
          context, tflite::XNNPackQS8Options::default_value);
    });
  }
#endif
}

}  // namespace optimization_guide
