// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/test_tflite_model_executor.h"

#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace optimization_guide {

bool TestTFLiteModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const std::vector<float>& input) {
  return tflite::task::core::PopulateTensor<float>(input, input_tensors[0])
      .ok();
}

std::optional<std::vector<float>> TestTFLiteModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  std::vector<float> data;
  absl::Status status =
      tflite::task::core::PopulateVector<float>(output_tensors[0], &data);
  DCHECK(status.ok());
  return data;
}

}  // namespace optimization_guide
