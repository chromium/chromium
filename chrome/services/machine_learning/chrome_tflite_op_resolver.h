// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MACHINE_LEARNING_CHROME_TFLITE_OP_RESOLVER_H_
#define CHROME_SERVICES_MACHINE_LEARNING_CHROME_TFLITE_OP_RESOLVER_H_

#include "third_party/tflite/src/tensorflow/lite/model.h"
#include "third_party/tflite/src/tensorflow/lite/mutable_op_resolver.h"

namespace machine_learning {

// This class maintains all the currently supported TFLite
// operations for Chrome and registers them for use.
class ChromeTFLiteOpResolver : public tflite::MutableOpResolver {
 public:
  ChromeTFLiteOpResolver();
};

}  // namespace machine_learning

#endif  // CHROME_SERVICES_MACHINE_LEARNING_CHROME_TFLITE_OP_RESOLVER_H_
