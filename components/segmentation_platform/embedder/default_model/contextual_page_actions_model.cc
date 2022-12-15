// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/contextual_page_actions_model.h"

#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for contextual page actions model.
constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING;

}  // namespace

ContextualPageActionsModel::ContextualPageActionsModel()
    : ModelProvider(kSegmentId) {}

void ContextualPageActionsModel::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      /*min_signal_collection_length_days=*/1,
      /*signal_storage_length_days=*/1);

  // Add price tracking custom input.
  proto::CustomInput* price_tracking_input =
      writer.AddCustomInput(MetadataWriter::CustomInput{
          .tensor_length = 1,
          .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
          .name = "price_tracking_input"});
  (*price_tracking_input->mutable_additional_args())["name"] =
      kContextualPageActionModelInputPriceTracking;

  // Add reader mode custom input.
  proto::CustomInput* reader_mode_input =
      writer.AddCustomInput(MetadataWriter::CustomInput{
          .tensor_length = 1,
          .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
          .name = "reader_mode_input"});
  (*reader_mode_input->mutable_additional_args())["name"] =
      kContextualPageActionModelInputReaderMode;

  // A threshold used to differentiate labels with score zero from non-zero
  // values.
  const float threshold = 0.1f;

  // Set output config, labels, and classifier.
  writer.AddOutputConfigForMultiClassClassifier(
      {kContextualPageActionModelLabelPriceTracking,
       kContextualPageActionModelLabelReaderMode},
      /*top_k_outputs=*/1, threshold);

  constexpr int kModelVersion = 1;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindRepeating(model_updated_callback, kSegmentId,
                                     std::move(metadata), kModelVersion));
}

void ContextualPageActionsModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != 2) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  bool can_track_price = inputs[0];
  bool has_reader_mode = inputs[1];

  // Create response.
  ModelProvider::Response response(2, 0);
  response[0] = can_track_price;
  response[1] = has_reader_mode;
  // TODO(crbug/1399467): Set a classifier threshold.

  // TODO(shaktisahu): This class needs some rethinking to correctly associate
  // the labeled outputs to the flattened vector. Maybe have this method return
  // a map of labeled outputs which callls a superclass method internally to
  // flatten to the vector. Similar association is needed for the inputs as
  // well, but a different topic. Should have same kind of utility in python /
  // server side as well.

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

bool ContextualPageActionsModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
