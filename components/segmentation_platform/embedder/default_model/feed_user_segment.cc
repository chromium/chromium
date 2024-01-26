// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/feed_user_segment.h"

#include <array>

#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {

using proto::SegmentId;

// Default parameters for feed user model.
constexpr int kModelVersion = 2;
constexpr SegmentId kFeedUserSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER;
constexpr int64_t kFeedUserSignalStorageLength = 28;
constexpr int64_t kFeedUserMinSignalCollectionLength = 7;
constexpr int kFeedUserSegmentSelectionTTLDays = 14;

// InputFeatures.

// The enum values are based on feed::FeedEngagementType.
constexpr std::array<int32_t, 1> kFeedEngagementEngaged{0};
constexpr std::array<int32_t, 1> kFeedEngagementSimple{1};
constexpr std::array<int32_t, 1> kFeedEngagementInteracted{2};
constexpr std::array<int32_t, 1> kFeedEngagementScrolled{4};

constexpr std::array<MetadataWriter::UMAFeature, 11> kFeedUserUMAFeatures = {
    MetadataWriter::UMAFeature::FromUserAction("MobileNTPMostVisited", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileNewTabOpened", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileNewTabShown", 14),
    MetadataWriter::UMAFeature::FromUserAction("Home", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuRecentTabs", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuHistory", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileTabReturnedToCurrentTab",
                                               14),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "ContentSuggestions.Feed.EngagementType",
        28,
        kFeedEngagementEngaged.data(),
        kFeedEngagementEngaged.size()),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "ContentSuggestions.Feed.EngagementType",
        28,
        kFeedEngagementSimple.data(),
        kFeedEngagementSimple.size()),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "ContentSuggestions.Feed.EngagementType",
        28,
        kFeedEngagementInteracted.data(),
        kFeedEngagementInteracted.size()),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "ContentSuggestions.Feed.EngagementType",
        28,
        kFeedEngagementScrolled.data(),
        kFeedEngagementScrolled.size()),
};

std::unique_ptr<DefaultModelProvider> GetFeedUserSegmentDefautlModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          features::kSegmentationPlatformFeedSegmentFeature,
          kDefaultModelEnabledParam, true)) {
    return nullptr;
  }
  return std::make_unique<FeedUserSegment>();
}

}  // namespace

// static
std::unique_ptr<Config> FeedUserSegment::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformFeedSegmentFeature)) {
    return nullptr;
  }

  auto config = std::make_unique<Config>();
  config->segmentation_key = kFeedUserSegmentationKey;
  config->segmentation_uma_name = kFeedUserSegmentUmaName;
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER,
                       GetFeedUserSegmentDefautlModel());
  config->auto_execute_and_cache = true;
  return config;
}

FeedUserSegment::FeedUserSegment() : DefaultModelProvider(kFeedUserSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
FeedUserSegment::GetModelConfig() {
  proto::SegmentationModelMetadata chrome_start_metadata;
  MetadataWriter writer(&chrome_start_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kFeedUserMinSignalCollectionLength, kFeedUserSignalStorageLength);

  // Set features.
  writer.AddUmaFeatures(kFeedUserUMAFeatures.data(),
                        kFeedUserUMAFeatures.size());

  //  Set OutputConfig.
  writer.AddOutputConfigForBinaryClassifier(
      /*threshold=*/0.5f, /*positive_label=*/
      SegmentIdToHistogramVariant(kFeedUserSegmentId),
      /*negative_label=*/kLegacyNegativeLabel);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{}, kFeedUserSegmentSelectionTTLDays,
      /*time_unit=*/proto::TimeUnit::DAY);

  return std::make_unique<ModelConfig>(std::move(chrome_start_metadata),
                                       kModelVersion);
}

void FeedUserSegment::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kFeedUserUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  const bool feed_engaged = inputs[7] >= 2;
  const bool feed_engaged_simple = inputs[8] >= 2;
  const bool feed_interacted = inputs[9] >= 2;
  const bool feed_scrolled = inputs[10] >= 2;
  float result = 0;

  if (feed_engaged || feed_engaged_simple || feed_interacted || feed_scrolled) {
    result = 1;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
