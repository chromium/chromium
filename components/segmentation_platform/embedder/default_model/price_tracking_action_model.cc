// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/price_tracking_action_model.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for price tracking action model.
constexpr SegmentId kPriceTrackingSegmentId =
    SegmentId::OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING;

}  // namespace

PriceTrackingActionModel::PriceTrackingActionModel()
    : ModelProvider(kPriceTrackingSegmentId) {}

void PriceTrackingActionModel::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      /*min_signal_collection_length_days=*/1,
      /*signal_storage_length_days=*/1);

  // Add price tracking custom input.
  proto::CustomInput* custom_input =
      writer.AddCustomInput(MetadataWriter::CustomInput{
          .tensor_length = 1,
          .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
          .name = "price_tracking"});
  (*custom_input->mutable_additional_args())["name"] = "is_price_tracking";

  // Set discrete mapping.
  writer.AddBooleanSegmentDiscreteMapping(kContextualPageActionsKey);

  constexpr int kModelVersion = 1;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindRepeating(model_updated_callback, kPriceTrackingSegmentId,
                          std::move(metadata), kModelVersion));
}

void PriceTrackingActionModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != 1) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  // Input[0] is price tracking enabled, which is also the result.
  float result = inputs[0];

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

bool PriceTrackingActionModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
