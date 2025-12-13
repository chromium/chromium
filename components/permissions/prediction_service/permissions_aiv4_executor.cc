// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv4_executor.h"

#include <array>
#include <vector>

#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/permissions/prediction_service/permissions_aiv4_model_metadata.pb.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace permissions {

using ModelInput = PermissionsAiv4Executor::ModelInput;
using ModelOutput = PermissionsAiv4Executor::ModelOutput;
using ::passage_embeddings::Embedding;
using ::tflite::task::core::PopulateTensor;

// The default size of the text input tensor for the model. This is
// necessary if the model metadata does not provide this value.
constexpr int kDefaultTextInputSize = 768;

PermissionsAiv4ExecutorInput::PermissionsAiv4ExecutorInput(
    SkBitmap snapshot,
    Embedding inner_text_embedding)
    : snapshot(snapshot), inner_text_embedding(inner_text_embedding) {}

PermissionsAiv4ExecutorInput::~PermissionsAiv4ExecutorInput() = default;
PermissionsAiv4ExecutorInput::PermissionsAiv4ExecutorInput(
    const PermissionsAiv4ExecutorInput&) = default;
PermissionsAiv4ExecutorInput::PermissionsAiv4ExecutorInput(
    PermissionsAiv4ExecutorInput&&) = default;

bool PermissionsAiv4Executor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const ModelInput& input) {
  DCHECK(input_tensors.size() == 2);

  int expected_input_size = kDefaultTextInputSize;
  if (input.metadata.has_value() &&
      input.metadata.value().has_text_embeddings_input_size()) {
    expected_input_size = input.metadata.value().text_embeddings_input_size();
  }

  const auto& embedding = input.inner_text_embedding;
  if (static_cast<int>(embedding.Dimensions()) != expected_input_size) {
    VLOG(1)
        << "[PermissionsAiv4Executor]: Input Size does not match expectations: "
        << embedding.Dimensions() << " vs (expected) " << expected_input_size;
    return false;
  }
  if (!PopulateTensor<float>(embedding.GetData().data(), expected_input_size,
                             input_tensors[0])
           .ok()) {
    VLOG(1) << "[PermissionsAiv4Executor]: Failed to copy passage "
               "embedding.";
    return false;
  }
  if (!ConvertSkBitMapToTfliteTensor(input_tensors[1], input.snapshot)) {
    VLOG(1)
        << "[PermissionsAiv4Executor]: Failed to convert skbitmap to tflite "
           "tensor data.";
    return false;
  }
  VLOG(1) << "[PermissionsAiv4Executor]: Successfully encoded input!";
  SetThresholdValues(input.metadata);
  return true;
}

void PermissionsAiv4Executor::SetThresholdValues(
    base::optional_ref<const PermissionsAiv4ModelMetadata> metadata) {
  if (!metadata.has_value() || !metadata.value().has_relevance_thresholds()) {
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
  const auto& thresholds = metadata.value().relevance_thresholds();
  relevance_thresholds() = {
      thresholds.min_low_relevance(), thresholds.min_medium_relevance(),
      thresholds.min_high_relevance(), thresholds.min_very_high_relevance()};
}

}  // namespace permissions
