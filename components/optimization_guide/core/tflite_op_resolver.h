// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_TFLITE_OP_RESOLVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_TFLITE_OP_RESOLVER_H_

#include "third_party/tflite/src/tensorflow/lite/model.h"
#include "third_party/tflite/src/tensorflow/lite/mutable_op_resolver.h"

namespace optimization_guide {

// This class maintains all the currently supported TFLite
// operations for the Chromium build of TFLite and registers them for use.
class TFLiteOpResolver : public tflite::MutableOpResolver {
 public:
  TFLiteOpResolver();
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_TFLITE_OP_RESOLVER_H_
