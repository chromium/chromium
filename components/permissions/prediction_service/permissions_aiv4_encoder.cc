// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv4_encoder.h"

#include <array>
#include <vector>

#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace permissions {

using ModelInput = PermissionsAiv4Encoder::ModelInput;
using ModelOutput = PermissionsAiv4Encoder::ModelOutput;
constexpr int kTextInputSize = PermissionsAiv4Encoder::kTextInputSize;
using ::passage_embeddings::Embedding;
using ::tflite::task::core::PopulateTensor;

PermissionsAiv4EncoderInput::PermissionsAiv4EncoderInput(
    SkBitmap snapshot,
    Embedding inner_text_embedding)
    : snapshot(snapshot), inner_text_embedding(inner_text_embedding) {}

PermissionsAiv4EncoderInput::~PermissionsAiv4EncoderInput() = default;
PermissionsAiv4EncoderInput::PermissionsAiv4EncoderInput(
    const PermissionsAiv4EncoderInput&) = default;
PermissionsAiv4EncoderInput::PermissionsAiv4EncoderInput(
    PermissionsAiv4EncoderInput&&) = default;

bool CopyPassageEmbeddingIntoInputTensor(TfLiteTensor* input_tensor,
                                         const Embedding& embedding) {
  if (embedding.Dimensions() != kTextInputSize) {
    // TODO(crbug.com/382447738) We need to synchronize this via metadata with
    // passage_embedder; the embedders output size might change in the future
    // and at the moment information of their models output size is provided via
    // model metadata. We should not use a constant here, but also provide the
    // expected input size of our model via a metadata object.
    VLOG(1)
        << "[PermissionsAIv4Encoder]: Input Size does not match expectations: "
        << embedding.Dimensions() << " vs (expected) " << kTextInputSize;
    return false;
  }
  return PopulateTensor<float>(embedding.GetData().data(), kTextInputSize,
                               input_tensor)
      .ok();
}

bool PermissionsAiv4Encoder::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const ModelInput& input) {
  DCHECK(input_tensors.size() == 2);

  if (!CopyPassageEmbeddingIntoInputTensor(input_tensors[0],
                                           input.inner_text_embedding)) {
    VLOG(1) << "[PermissionsAIv4Encoder]: Failed to copy passage "
               "embedding.";
    return false;
  }
  if (!ConvertSkBitMapToTfliteTensor(input_tensors[1], input.snapshot)) {
    VLOG(1) << "[PermissionsAIv4Encoder]: Failed to convert skbitmap to tflite "
               "tensor data.";
    return false;
  }
  VLOG(1) << "[PermissionsAIv4Encoder]: Successfully encoded input!";
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
