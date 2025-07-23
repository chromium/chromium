// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv4_encoder.h"

#include <array>
#include <vector>

#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace permissions {

using ModelInput = PermissionsAiv4Encoder::ModelInput;
using ModelOutput = PermissionsAiv4Encoder::ModelOutput;
using ::tflite::task::core::PopulateTensor;

PermissionsAiv4EncoderInput::PermissionsAiv4EncoderInput() = default;
PermissionsAiv4EncoderInput::~PermissionsAiv4EncoderInput() = default;
PermissionsAiv4EncoderInput::PermissionsAiv4EncoderInput(
    const PermissionsAiv4EncoderInput&) = default;

bool PermissionsAiv4Encoder::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const ModelInput& input) {
  DCHECK(input_tensors.size() == 2);

  // TODO(crbug.com/382447738) Create embedding for rendered text

  if (!ConvertSkBitMapToTfliteTensor(input_tensors[1], input.snapshot)) {
    return false;
  }
  SetThresholdValues();
  return true;
}

void PermissionsAiv4Encoder::SetThresholdValues() {
  DCHECK(request_type() == RequestType::kNotifications ||
         request_type() == RequestType::kGeolocation);

  // Empirically determined thresholds, that map to relevance enum vals as
  // follows:
  // val < thr[0] -> VeryLow
  // ...
  // val < thr[4] -> High
  // val >= thr[4] -> VeryHigh
  relevance_thresholds() = {0.008f, 0.024f, 0.11f, 0.32f};
  if (request_type() == RequestType::kGeolocation) {
    relevance_thresholds() = {0.033f, 0.077f, 0.2f, 0.49f};
  }
  return;
}

}  // namespace permissions
