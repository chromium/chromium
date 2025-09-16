// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/segmentation_platform/embedder/default_model/tips_notifications_ranker.h"

#include <memory>

#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for TipsNotificationsRanker model.
constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_TIPS_NOTIFICATIONS_RANKER;
constexpr int64_t kModelVersion = 1;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// Wait until we have 7 days of data.
constexpr int64_t kMinSignalCollectionLength = 7;

constexpr std::array<const char*, 4> kTipsNotificationsLabels = {
    kEnhancedSafeBrowsing, kQuickDelete, kGoogleLens, kBottomOmnibox};

// InputFeatures.

// Enum values for histograms.
constexpr std::array<int32_t, 1> kEnumValueForEnhancedSafeBrowsingUsage{
    /*EnhancedSafeBrowsing=*/1};

constexpr std::array<int32_t, 1> kEnumValueForQuickDeleteMagicStackImpression{
    /*QuickDelete=*/9};

constexpr std::array<int32_t, 1> kEnumValueForBottomOmniboxUsage{
    /*BottomOmnibox=*/2};

// Set UMA metrics to use as input.
constexpr std::array<MetadataWriter::UMAFeature, 4> kUMAFeatures = {
    // EnhancedSafeBrowsing
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "SafeBrowsing.Settings.UserAction.Default",
        28,
        kEnumValueForEnhancedSafeBrowsingUsage.data(),
        kEnumValueForEnhancedSafeBrowsingUsage.size()),
    // QuickDelete MagicStackImpression
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2",
        28,
        kEnumValueForQuickDeleteMagicStackImpression.data(),
        kEnumValueForQuickDeleteMagicStackImpression.size()),
    // GoogleLens
    MetadataWriter::UMAFeature::FromUserAction("NewTabPage.SearchBox.Lens", 28),
    // BottomOmnibox
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Android.ToolbarPosition.PositionPrefChanged",
        28,
        kEnumValueForBottomOmniboxUsage.data(),
        kEnumValueForBottomOmniboxUsage.size()),
};

}  // namespace

// static
std::unique_ptr<Config> TipsNotificationsRanker::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformTipsNotificationsRanker)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kTipsNotificationsRankerKey;
  config->segmentation_uma_name = kTipsNotificationsRankerUmaName;
  config->AddSegmentId(kSegmentId, std::make_unique<TipsNotificationsRanker>());
  config->auto_execute_and_cache = false;
  return config;
}

TipsNotificationsRanker::TipsNotificationsRanker()
    : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
TipsNotificationsRanker::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);
  metadata.set_upload_tensors(false);

  // Set output config.
  writer.AddOutputConfigForMultiClassClassifier(kTipsNotificationsLabels,
                                                kTipsNotificationsLabels.size(),
                                                /*threshold=*/0.5);

  // Set features.
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void TipsNotificationsRanker::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  ModelProvider::Response response(kTipsNotificationsLabels.size(), 0);
  // TODO(crbug.com/444281425): Replace default ranking with evaluation logic.
  response[0] = 0;  // EnhancedSafeBrowsing
  response[1] = 0;  // QuickDelete
  response[2] = 0;  // GoogleLens
  response[3] = 0;  // BottomOmnibox

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}
}  // namespace segmentation_platform
