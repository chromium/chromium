// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/optimization_guide/segmentation_model_executor.h"

#include <vector>

#include "base/check_op.h"
#include "third_party/tflite/src/tensorflow/lite/c/common.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace segmentation_platform {

SegmentationModelExecutor::SegmentationModelExecutor() = default;

SegmentationModelExecutor::~SegmentationModelExecutor() = default;

bool SegmentationModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const ModelProvider::Request& input) {
  // The model must have a single float input tensor, and the length of the
  // input data must match the length of the tensor.
  if (input_tensors.size() != 1u) {
    LOG(ERROR) << "input tensor size not 1";
    return false;
  }
  if (kTfLiteFloat32 != input_tensors[0]->type) {
    LOG(ERROR) << "input tensor type is not float";
    return false;
  }
  if (input_tensors[0]->bytes / sizeof(input_tensors[0]->type) !=
      input.size()) {
    LOG(ERROR) << "length of input data does not match length of tensor";
    return false;
  }

  absl::Status status =
      tflite::task::core::PopulateTensor<float>(input, input_tensors[0]);
  return status.ok();
}

std::optional<ModelProvider::Response> SegmentationModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  // The output must be a single tensor with float elements.
  DCHECK_EQ(1u, output_tensors.size());
  DCHECK_EQ(kTfLiteFloat32, output_tensors[0]->type);
  ModelProvider::Response data;
  absl::Status status =
      tflite::task::core::PopulateVector<float>(output_tensors[0], &data);
  if (!status.ok()) {
    NOTREACHED_IN_MIGRATION();
    return std::nullopt;
  }
  return data;
}

}  // namespace segmentation_platform
