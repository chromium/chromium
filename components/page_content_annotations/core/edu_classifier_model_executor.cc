// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/edu_classifier_model_executor.h"

#include <optional>
#include <vector>

#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace page_content_annotations {

EduClassifierModelExecutor::EduClassifierModelExecutor() = default;
EduClassifierModelExecutor::~EduClassifierModelExecutor() = default;

bool EduClassifierModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    ModelInput input) {
  if (input_tensors.size() != 1u) {
    return false;
  }
  if (input_tensors[0]->type != kTfLiteFloat32) {
    return false;
  }

  return tflite::task::core::PopulateTensor<float>(input, input_tensors[0])
      .ok();
}

std::optional<EduClassifierModelExecutor::ModelOutput>
EduClassifierModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  if (output_tensors.size() != 1u) {
    return std::nullopt;
  }
  if (output_tensors[0]->type != kTfLiteFloat32) {
    return std::nullopt;
  }

  std::vector<float> output;
  if (!tflite::task::core::PopulateVector<float>(output_tensors[0], &output)
           .ok()) {
    return std::nullopt;
  }
  if (output.size() != 1u) {
    return std::nullopt;
  }
  return output[0];
}

}  // namespace page_content_annotations
