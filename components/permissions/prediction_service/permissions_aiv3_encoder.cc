// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv3_encoder.h"

#include <array>
#include <vector>

#include "skia/ext/image_operations.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace permissions {

namespace {
using ModelInput = PermissionsAiv3Encoder::ModelInput;
using ModelOutput = PermissionsAiv3Encoder::ModelOutput;
using ::tflite::task::core::PopulateTensor;

PermissionRequestRelevance ConvertToRelevance(
    float val,
    const std::array<float, 4>& thresholds) {
  // Empirically determined thresholds, that map to relevance enum vals as
  // follows:
  // val < thr[0] -> VeryLow
  // ...
  // val < thr[4] -> High
  // val >= thr[4] -> VeryHigh
  for (size_t i = 0; i < thresholds.size(); ++i) {
    if (val < thresholds[i]) {
      return static_cast<PermissionRequestRelevance>(i + 1);
    }
  }
  return PermissionRequestRelevance::kVeryHigh;
}

}  // namespace

const int PermissionsAiv3Encoder::kModelInputWidth = 64;
const int PermissionsAiv3Encoder::kModelInputHeight = 64;

bool PermissionsAiv3Encoder::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const ModelInput& input) {
  // TODO(crbug.com/405095664): Figure out if resize_best is fast enough to deal
  // with too big/small inputs.
  SkBitmap resized =
      skia::ImageOperations::Resize(input, skia::ImageOperations::RESIZE_BEST,
                                    kModelInputWidth, kModelInputHeight);
  if (resized.drawsNothing()) {
    return false;
  }

  std::array<float, kModelInputHeight * kModelInputWidth * 3> data;
  int index = 0;
  for (int h = 0; h < resized.height(); ++h) {
    for (int w = 0; w < resized.width(); ++w) {
      SkColor color = resized.getColor(h, w);
      // We normalize the pixel values to be in between 0 and 1.
      // TODO(crbug.com/405095664): We need to investigate if this is the
      // correct way to fill the tensors data;
      data[index++] = static_cast<float>(SkColorGetR(color)) / 255.0f;
      data[index++] = static_cast<float>(SkColorGetG(color)) / 255.0f;
      data[index++] = static_cast<float>(SkColorGetB(color)) / 255.0f;
    }
  }
  if (!PopulateTensor<float>(data.data(), data.size(), input_tensors[0]).ok()) {
    return false;
  }
  return true;
}

std::optional<ModelOutput> PermissionsAiv3Encoder::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  DCHECK(request_type_ == RequestType::kNotifications ||
         request_type_ == RequestType::kGeolocation);

  // TODO(crbug.com/405095664): should be fetched via the model metadata proto
  // as soon as we have this.
  static constexpr std::array<float, 4> geolocation_thresholds = {0.2f, 0.4f,
                                                                  0.5f, 0.65f};
  static constexpr std::array<float, 4> notification_thresholds = {0.2f, 0.4f,
                                                                   0.7f, 0.84f};

  std::vector<float> data;
  if (!tflite::task::core::PopulateVector<float>(output_tensors[0], &data)
           .ok()) {
    return std::nullopt;
  }

  return ConvertToRelevance(data[0],
                            request_type_ == RequestType::kNotifications
                                ? notification_thresholds
                                : geolocation_thresholds);
}

}  // namespace permissions
