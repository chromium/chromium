// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_EMBEDDING_LOOKUP_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_EMBEDDING_LOOKUP_H_

#include "third_party/tflite/src/tensorflow/lite/kernels/register.h"

namespace language_detection {

// This op takes in a list of indices, and for each index, it looks up the
// corresponding embedding in the given embedding table, and computes the mean
// embedding. The mean embedding is the output of this op.
//
// This op supports compressed embedding table representations (currently
// limited to n-bit quantization).
TfLiteRegistration* Register_EMBEDDING_LOOKUP();

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_EMBEDDING_LOOKUP_H_
