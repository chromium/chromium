// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv3_encoder.h"

#include <array>
#include <vector>

#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/permissions/prediction_service/permissions_aiv3_model_metadata.pb.h"
#include "skia/ext/image_operations.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace permissions {

using ModelInput = PermissionsAiv3Encoder::ModelInput;
using ModelOutput = PermissionsAiv3Encoder::ModelOutput;
using ::tflite::task::core::PopulateTensor;

const int PermissionsAiv3Encoder::kModelInputWidth = 64;
const int PermissionsAiv3Encoder::kModelInputHeight = 64;

PermissionsAiv3EncoderInput::PermissionsAiv3EncoderInput() = default;
PermissionsAiv3EncoderInput::~PermissionsAiv3EncoderInput() = default;
PermissionsAiv3EncoderInput::PermissionsAiv3EncoderInput(
    const PermissionsAiv3EncoderInput&) = default;

bool PermissionsAiv3Encoder::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const ModelInput& input) {
  SkBitmap resized = skia::ImageOperations::Resize(
      input.snapshot, skia::ImageOperations::RESIZE_BEST, kModelInputWidth,
      kModelInputHeight);
  if (resized.drawsNothing()) {
    return false;
  }

  std::array<float, kModelInputHeight * kModelInputWidth * 3> data;

  int index = 0;
  for (int h = 0; h < resized.height(); ++h) {
    for (int w = 0; w < resized.width(); ++w) {
      SkColor color = resized.getColor(h, w);
      // We normalize the pixel values to be in between 0 and 1.
      data[index++] = static_cast<float>(SkColorGetR(color)) / 255.0f;
      data[index++] = static_cast<float>(SkColorGetG(color)) / 255.0f;
      data[index++] = static_cast<float>(SkColorGetB(color)) / 255.0f;
    }
  }
  if (!PopulateTensor<float>(data.data(), data.size(), input_tensors[0]).ok()) {
    return false;
  }
  SetThresholdValues(input.metadata);
  return true;
}

std::optional<ModelOutput> PermissionsAiv3Encoder::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  std::vector<float> data;
  if (!tflite::task::core::PopulateVector<float>(output_tensors[0], &data)
           .ok()) {
    return std::nullopt;
  }

  for (size_t i = 0; i < relevance_thresholds_.size(); ++i) {
    if (data[0] < relevance_thresholds_[i]) {
      return static_cast<PermissionRequestRelevance>(i + 1);
    }
  }
  return PermissionRequestRelevance::kVeryHigh;
}

void PermissionsAiv3Encoder::SetThresholdValues(
    base::optional_ref<const PermissionsAiv3ModelMetadata> metadata) {
  if (!metadata.has_value() || !metadata.value().has_relevance_thresholds()) {
    DCHECK(request_type_ == RequestType::kNotifications ||
           request_type_ == RequestType::kGeolocation);

    // Empirically determined thresholds, that map to relevance enum vals as
    // follows:
    // val < thr[0] -> VeryLow
    // ...
    // val < thr[4] -> High
    // val >= thr[4] -> VeryHigh
    relevance_thresholds_ = {0.2f, 0.4f, 0.7f, 0.84f};
    if (request_type_ == RequestType::kGeolocation) {
      relevance_thresholds_ = {0.2f, 0.4f, 0.5f, 0.65f};
    }
    return;
  }
  const auto& thresholds = metadata.value().relevance_thresholds();
  relevance_thresholds_ = {
      thresholds.min_low_relevance(), thresholds.min_medium_relevance(),
      thresholds.min_high_relevance(), thresholds.min_very_high_relevance()};
}

base::TaskPriority PermissionsAiv3Encoder::GetModelLoadingTaskPriority() const {
  return base::TaskPriority::USER_VISIBLE;
}

}  // namespace permissions
