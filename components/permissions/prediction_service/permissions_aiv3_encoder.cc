// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv3_encoder.h"

#include <array>
#include <vector>

#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/permissions/prediction_service/permissions_aiv3_model_metadata.pb.h"

namespace permissions {
PermissionsAiv3EncoderInput::~PermissionsAiv3EncoderInput() = default;
PermissionsAiv3EncoderInput::PermissionsAiv3EncoderInput() = default;
PermissionsAiv3EncoderInput::PermissionsAiv3EncoderInput(
    const PermissionsAiv3EncoderInput&) = default;
PermissionsAiv3EncoderInput::PermissionsAiv3EncoderInput(
    PermissionsAiv3EncoderInput&&) = default;
PermissionsAiv3EncoderInput::PermissionsAiv3EncoderInput(SkBitmap snapshot)
    : snapshot(std::move(snapshot)) {}

bool PermissionsAiv3Encoder::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const ModelInput& input) {
  if (!ConvertSkBitMapToTfliteTensor(input_tensors[0], input.snapshot)) {
    return false;
  }
  SetThresholdValues(input.metadata);
  return true;
}

void PermissionsAiv3Encoder::SetThresholdValues(
    base::optional_ref<const PermissionsAiv3ModelMetadata> metadata) {
  if (!metadata.has_value() || !metadata.value().has_relevance_thresholds()) {
    DCHECK(request_type() == RequestType::kNotifications ||
           request_type() == RequestType::kGeolocation);

    // Empirically determined thresholds, that map to relevance enum vals as
    // follows:
    // val < thr[0] -> VeryLow
    // ...
    // val < thr[4] -> High
    // val >= thr[4] -> VeryHigh
    relevance_thresholds() = {0.2f, 0.4f, 0.7f, 0.84f};
    if (request_type() == RequestType::kGeolocation) {
      relevance_thresholds() = {0.2f, 0.4f, 0.5f, 0.65f};
    }
    return;
  }
  const auto& thresholds = metadata.value().relevance_thresholds();
  relevance_thresholds() = {
      thresholds.min_low_relevance(), thresholds.min_medium_relevance(),
      thresholds.min_high_relevance(), thresholds.min_very_high_relevance()};
}

}  // namespace permissions
