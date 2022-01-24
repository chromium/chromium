// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/test_model_executor.h"

#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace optimization_guide {

absl::Status TestModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const std::vector<float>& input) {
  tflite::task::core::PopulateTensor<float>(input, input_tensors[0]);
  return absl::OkStatus();
}

std::vector<float> TestModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  std::vector<float> data;
  tflite::task::core::PopulateVector<float>(output_tensors[0], &data);
  return data;
}

}  // namespace optimization_guide
