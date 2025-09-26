// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_ai_encoder_base.h"

#include <array>
#include <vector>

#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/permissions/prediction_service/permissions_aiv3_executor.h"
#include "components/permissions/prediction_service/permissions_aiv4_executor.h"
#include "skia/ext/image_operations.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace permissions {

namespace {
constexpr int kImageInputWidth = 64;
constexpr int kImageInputHeight = 64;
}  // namespace

template <typename EncoderInput>
const int PermissionsAiEncoderBase<EncoderInput>::kImageInputHeight =
    permissions::kImageInputHeight;
template <typename EncoderInput>
const int PermissionsAiEncoderBase<EncoderInput>::kImageInputWidth =
    permissions::kImageInputWidth;

using ::tflite::task::core::PopulateTensor;

template <typename EncoderInput>
bool PermissionsAiEncoderBase<EncoderInput>::ConvertSkBitMapToTfliteTensor(
    TfLiteTensor* input_tensor,
    const SkBitmap& input) {
  SkBitmap resized =
      skia::ImageOperations::Resize(input, skia::ImageOperations::RESIZE_BEST,
                                    kImageInputWidth, kImageInputHeight);
  if (resized.drawsNothing()) {
    return false;
  }

  std::array<float,
             permissions::kImageInputWidth * permissions::kImageInputHeight * 3>
      data;

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
  return PopulateTensor<float>(data.data(), data.size(), input_tensor).ok();
}

template <typename EncoderInput>
std::optional<typename PermissionsAiEncoderBase<EncoderInput>::ModelOutput>
PermissionsAiEncoderBase<EncoderInput>::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  std::vector<float> data;
  if (!tflite::task::core::PopulateVector<float>(output_tensors[0], &data)
           .ok()) {
    return std::nullopt;
  }
  // Uses empirically determined thresholds, that map to relevance enum vals as
  // follows:
  // val < thr[0] -> VeryLow
  // ...
  // val < thr[4] -> High
  // val >= thr[4] -> VeryHigh
  for (size_t i = 0; i < relevance_thresholds_.size(); ++i) {
    if (data[0] < relevance_thresholds_[i]) {
      return static_cast<PermissionRequestRelevance>(i + 1);
    }
  }

  return PermissionRequestRelevance::kVeryHigh;
}

// Template instantiation for the Aiv3/Aiv4 model handlers.
template class PermissionsAiEncoderBase<const PermissionsAiv3ExecutorInput&>;
template class PermissionsAiEncoderBase<const PermissionsAiv4ExecutorInput&>;

}  // namespace permissions
