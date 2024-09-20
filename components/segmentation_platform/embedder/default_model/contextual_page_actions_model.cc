// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/contextual_page_actions_model.h"

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Label input size
constexpr int kLabelInputSize = 4;
// Default parameters for contextual page actions model.
constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING;
constexpr int64_t kOneDayInSeconds = 86400;
// Parameters for share action model.
constexpr std::array<MetadataWriter::UMAFeature, 6> kShareUMAFeatures = {
    MetadataWriter::UMAFeature::FromUserAction(
        "MobileMenuShare",
        ContextualPageActionsModel::kShareOutputCollectionDelayInSec),
    MetadataWriter::UMAFeature::FromUserAction(
        "Omnibox.EditUrlSuggestion.Share",
        ContextualPageActionsModel::kShareOutputCollectionDelayInSec),
    MetadataWriter::UMAFeature::FromUserAction(
        "MobileActionMode.Share",
        ContextualPageActionsModel::kShareOutputCollectionDelayInSec),
    MetadataWriter::UMAFeature::FromUserAction(
        "MobileMenuDirectShare",
        ContextualPageActionsModel::kShareOutputCollectionDelayInSec),
    MetadataWriter::UMAFeature::FromUserAction(
        "Omnibox.EditUrlSuggestion.Copy",
        ContextualPageActionsModel::kShareOutputCollectionDelayInSec),
    MetadataWriter::UMAFeature::FromUserAction(
        "Tab.Screenshot",
        ContextualPageActionsModel::kShareOutputCollectionDelayInSec),
};

constexpr std::array<const char*, kLabelInputSize>
    kContextualPageActionModelLabels = {
        kContextualPageActionModelLabelDiscounts,
        kContextualPageActionModelLabelPriceInsights,
        kContextualPageActionModelLabelPriceTracking,
        kContextualPageActionModelLabelReaderMode};

MetadataWriter::CustomInput CreateCustomInput(std::string name) {
  return MetadataWriter::CustomInput{
      .tensor_length = 1,
      .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
      .name = name.c_str()};
}

}  // namespace

ContextualPageActionsModel::ContextualPageActionsModel()
    : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
ContextualPageActionsModel::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetSegmentationMetadataConfig(
      proto::TimeUnit::SECOND, /*bucket_duration=*/1,
      /*signal_storage_length=*/kOneDayInSeconds,
      /*min_signal_collection_length=*/kOneDayInSeconds,
      /*result_time_to_live=*/kOneDayInSeconds);

  // Add discounts custom input.
  proto::CustomInput* discounts_input =
      writer.AddCustomInput(CreateCustomInput("discounts_input"));
  (*discounts_input->mutable_additional_args())["name"] =
      kContextualPageActionModelInputDiscounts;

  // Add price insights custom input.
  proto::CustomInput* price_insights_input =
      writer.AddCustomInput(CreateCustomInput("price_insights_input"));
  (*price_insights_input->mutable_additional_args())["name"] =
      kContextualPageActionModelInputPriceInsights;

  // Add price tracking custom input.
  proto::CustomInput* price_tracking_input =
      writer.AddCustomInput(CreateCustomInput("price_tracking_input"));
  (*price_tracking_input->mutable_additional_args())["name"] =
      kContextualPageActionModelInputPriceTracking;

  // Add reader mode custom input.
  proto::CustomInput* reader_mode_input =
      writer.AddCustomInput(CreateCustomInput("reader_mode_input"));
  (*reader_mode_input->mutable_additional_args())["name"] =
      kContextualPageActionModelInputReaderMode;

  if (base::FeatureList::IsEnabled(features::kContextualPageActionShareModel)) {
    // Add share related input features.
    writer.AddUmaFeatures(kShareUMAFeatures.data(), kShareUMAFeatures.size(),
                          false);

    metadata.set_upload_tensors(true);

    // Add share output collection with delay.
    writer.AddDelayTrigger(
        ContextualPageActionsModel::kShareOutputCollectionDelayInSec);
    writer.AddUmaFeatures(kShareUMAFeatures.data(), kShareUMAFeatures.size(),
                          true);
  }

  // A threshold used to differentiate labels with score zero from non-zero
  // values.
  const float threshold = 0.1f;

  // Set output config, labels, and classifier.
  writer.AddOutputConfigForMultiClassClassifier(
      kContextualPageActionModelLabels,
      /*top_k_outputs=*/1, threshold);

  constexpr int kModelVersion = 1;
  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void ContextualPageActionsModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  size_t expected_input_size =
      base::FeatureList::IsEnabled(features::kContextualPageActionShareModel)
          ? kShareUMAFeatures.size() + kLabelInputSize
          : kLabelInputSize;

  // Invalid inputs.
  if (inputs.size() != expected_input_size) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  // TODO(haileywang): Use input[4] to input[9] to show share button.
  bool has_discounts = inputs[0];
  bool has_price_insights = inputs[1];
  bool can_track_price = inputs[2];
  bool has_reader_mode = inputs[3];

  // Create response.
  ModelProvider::Response response(kLabelInputSize, 0);
  response[0] = has_discounts;
  response[1] = has_price_insights;
  response[2] = can_track_price;
  response[3] = has_reader_mode;
  // TODO(crbug.com/40249852): Set a classifier threshold.

  // TODO(shaktisahu): This class needs some rethinking to correctly associate
  // the labeled outputs to the flattened vector. Maybe have this method return
  // a map of labeled outputs which callls a superclass method internally to
  // flatten to the vector. Similar association is needed for the inputs as
  // well, but a different topic. Should have same kind of utility in python /
  // server side as well.

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

}  // namespace segmentation_platform
