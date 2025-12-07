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
constexpr int kLabelInputSize = 5;
// Default parameters for contextual page actions model.
constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING;
constexpr int64_t kOneDayInSeconds = 86400;

constexpr std::array<const char*, kLabelInputSize>
    kContextualPageActionModelLabels = {
        kContextualPageActionModelLabelDiscounts,
        kContextualPageActionModelLabelPriceInsights,
        kContextualPageActionModelLabelPriceTracking,
        kContextualPageActionModelLabelReaderMode,
        kContextualPageActionModelLabelTabGrouping};

// All stable buttons that can show in toolbar in Chrome tabbed activity.
constexpr std::array<int32_t, 7> kNonContextualActionEnumIds = {
    2,   // AdaptiveToolbarButtonVariant::kNewTab
    3,   // AdaptiveToolbarButtonVariant::kShare
    4,   // AdaptiveToolbarButtonVariant::kVoice
    8,   // AdaptiveToolbarButtonVariant::kTranslate
    9,   // AdaptiveToolbarButtonVariant::kAddToBookmarks
    10,  // AdaptiveToolbarButtonVariant::kReadAloud
    13,  // AdaptiveToolbarButtonVariant::kPageSummary
};

constexpr std::array<int32_t, 1> kTabGroupingEnumId = {
    16,  // AdaptiveToolbarButtonVariant::kTabGrouping
};

constexpr std::array<MetadataWriter::UMAFeature, 3> kUmaFeatures = {
    // For throttling based on non-contextual actions.
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Android.AdaptiveToolbarButton.Clicked",
        /*bucket_count=*/1,
        kNonContextualActionEnumIds.data(),
        kNonContextualActionEnumIds.size()),
    // For getting the shown count of tab grouping action.
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Android.AdaptiveToolbarButton.Variant.OnPageLoad",
        /*bucket_count=*/1,
        kTabGroupingEnumId.data(),
        kTabGroupingEnumId.size()),
    // For getting the clicked count of tab grouping action.
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Android.AdaptiveToolbarButton.Clicked",
        /*bucket_count=*/1,
        kTabGroupingEnumId.data(),
        kTabGroupingEnumId.size()),
};

MetadataWriter::CustomInput CreateCustomInput(const char* name) {
  return MetadataWriter::CustomInput{
      .tensor_length = 1,
      .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
      .name = name};
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
      /*min_signal_collection_length=*/0,
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

  // Add tab grouping cusotm input.
  proto::CustomInput* tab_grouping_input =
      writer.AddCustomInput(CreateCustomInput("tab_grouping_input"));
  (*tab_grouping_input->mutable_additional_args())["name"] =
      kContextualPageActionModelInputTabGrouping;

  writer.AddUmaFeatures(kUmaFeatures.data(), kUmaFeatures.size());

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
  // Invalid inputs.
  if (inputs.size() != kLabelInputSize + kUmaFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  bool has_discounts = inputs[0];
  bool has_price_insights = inputs[1];
  bool can_track_price = inputs[2];
  bool has_reader_mode = inputs[3];
  bool has_tab_grouping_suggestions = inputs[4];
  // Start of UMA features
  float non_contextual_click_count = inputs[kLabelInputSize + 0];
  float tab_group_shown_count = inputs[kLabelInputSize + 1];
  float tab_group_clicked_count = inputs[kLabelInputSize + 2];

  // Create response.
  ModelProvider::Response response(kLabelInputSize, 0);
  response[0] = has_discounts;
  response[1] = has_price_insights;
  response[2] = can_track_price;
  response[3] = has_reader_mode;

  bool show_tab_grouping = has_tab_grouping_suggestions;
  if (features::kContextualPageActionTabGroupParamThrottleOnNewTab.Get()) {
    show_tab_grouping &= (non_contextual_click_count == 0);
  }
  if (features::kContextualPageActionTabGroupParamShowWhenNotClickedInLastDay
          .Get()) {
    show_tab_grouping &=
        !(tab_group_shown_count > 0 && tab_group_clicked_count == 0);
  }
  response[4] = show_tab_grouping;

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
