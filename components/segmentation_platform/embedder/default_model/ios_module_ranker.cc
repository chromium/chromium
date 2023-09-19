// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/ios_module_ranker.h"

#include <memory>

#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for IosModuleRanker model.
constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_IOS_MODULE_RANKER;
constexpr int64_t kModelVersion = 3;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// Wait until we have 0 days of data.
constexpr int64_t kMinSignalCollectionLength = 0;
// Refresh the result every time.
constexpr int64_t kResultTTLMinutes = 1;

constexpr std::array<const char*, 5> kIosModuleLabels = {
    kMostVisitedTiles, kShortcuts, kSafetyCheck, kTabResumption,
    kParcelTracking};

constexpr std::array<const char*, 5> kIosModuleInputContextKeys = {
    kMostVisitedTilesFreshness, kShortcutsFreshness, kSafetyCheckFreshness,
    kTabResumptionFreshness, kParcelTrackingFreshness};

// InputFeatures.

// Enum values for the IOS.MagicStack.Module.Click and
// IOS.MagicStack.Module.TopImpression histograms.
constexpr std::array<int32_t, 1> kEnumValueForMVT{/*MostVisitedTiles=*/0};
constexpr std::array<int32_t, 1> kEnumValueForShortcuts{/*Shortcuts=*/1};
// TODO(ritikagup) : Update this if needed once histogram is available.
constexpr std::array<int32_t, 1> kEnumValueForSafetyCheck{/*SafetyCheck=*/7};
constexpr std::array<int32_t, 1> kEnumValueForTabResumption{
    /*TabResumption=*/8};
constexpr std::array<int32_t, 1> kEnumValueForParcelTracking{
    /*ParcelTracking=*/9};

// TODO(ritikagup) : Loop through all the modules for these features for better
// readability. Set UMA metrics to use as input.
constexpr std::array<MetadataWriter::UMAFeature, 30> kUMAFeatures = {
    // Most Visited Tiles
    // 0
    MetadataWriter::UMAFeature::FromEnumHistogram("IOS.MagicStack.Module.Click",
                                                  7,
                                                  kEnumValueForMVT.data(),
                                                  kEnumValueForMVT.size()),
    // 1
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.TopImpression",
        7,
        kEnumValueForMVT.data(),
        kEnumValueForMVT.size()),

    // Shortcuts
    // 2
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.Click",
        7,
        kEnumValueForShortcuts.data(),
        kEnumValueForShortcuts.size()),
    // 3
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.TopImpression",
        7,
        kEnumValueForShortcuts.data(),
        kEnumValueForShortcuts.size()),

    // Safety Check
    // 4
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.Click",
        7,
        kEnumValueForSafetyCheck.data(),
        kEnumValueForSafetyCheck.size()),
    // 5
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.TopImpression",
        7,
        kEnumValueForSafetyCheck.data(),
        kEnumValueForSafetyCheck.size()),

    // Most Visited Tiles
    // 6
    MetadataWriter::UMAFeature::FromEnumHistogram("IOS.MagicStack.Module.Click",
                                                  28,
                                                  kEnumValueForMVT.data(),
                                                  kEnumValueForMVT.size()),
    // 7
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.TopImpression",
        28,
        kEnumValueForMVT.data(),
        kEnumValueForMVT.size()),

    // Shortcuts
    // 8
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.Click",
        28,
        kEnumValueForShortcuts.data(),
        kEnumValueForShortcuts.size()),
    // 9
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.TopImpression",
        28,
        kEnumValueForShortcuts.data(),
        kEnumValueForShortcuts.size()),

    // Safety Check
    // 10
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.Click",
        28,
        kEnumValueForSafetyCheck.data(),
        kEnumValueForSafetyCheck.size()),
    // 11
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.TopImpression",
        28,
        kEnumValueForSafetyCheck.data(),
        kEnumValueForSafetyCheck.size()),

    // 12
    MetadataWriter::UMAFeature::FromUserAction(
        "MobileOmniboxShortcutsOpenMostVisitedItem",
        7),
    // 13
    MetadataWriter::UMAFeature::FromUserAction(
        "MobileOmniboxShortcutsOpenMostVisitedItem",
        28),
    // 14
    MetadataWriter::UMAFeature::FromUserAction(
        "MobileBookmarkManagerEntryOpened",
        7),
    // 15
    MetadataWriter::UMAFeature::FromUserAction(
        "MobileBookmarkManagerEntryOpened",
        28),
    // 16
    MetadataWriter::UMAFeature::FromUserAction(
        "MobileOmniboxShortcutsOpenReadingList",
        7),
    // 17
    MetadataWriter::UMAFeature::FromUserAction(
        "MobileOmniboxShortcutsOpenReadingList",
        28),
    // 18
    MetadataWriter::UMAFeature::FromUserAction("MobileReadingListOpen", 7),
    // 19
    MetadataWriter::UMAFeature::FromUserAction("MobileReadingListOpen", 28),
    // 20
    MetadataWriter::UMAFeature::FromUserAction("MobileReadingListAdd", 7),
    // 21
    MetadataWriter::UMAFeature::FromUserAction("MobileReadingListAdd", 28),

    // Tab Resumption
    // 22
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.Click",
        7,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),
    // 23
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.TopImpression",
        7,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),
    // 24
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.Click",
        28,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),
    // 25
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.TopImpression",
        28,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),

    // Parcel Tracking
    // 26
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.Click",
        7,
        kEnumValueForParcelTracking.data(),
        kEnumValueForParcelTracking.size()),
    // 27
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.TopImpression",
        7,
        kEnumValueForParcelTracking.data(),
        kEnumValueForParcelTracking.size()),
    // 28
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.Click",
        28,
        kEnumValueForParcelTracking.data(),
        kEnumValueForParcelTracking.size()),
    // 29
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "IOS.MagicStack.Module.TopImpression",
        28,
        kEnumValueForParcelTracking.data(),
        kEnumValueForParcelTracking.size()),
};

}  // namespace

// static
std::unique_ptr<Config> IosModuleRanker::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformIosModuleRanker)) {
    return nullptr;
  }
  bool serve_default_config = base::GetFieldTrialParamByFeatureAsBool(
      features::kSegmentationPlatformIosModuleRanker, kDefaultModelEnabledParam,
      false);
  auto config = std::make_unique<Config>();
  config->segmentation_key = kIosModuleRankerKey;
  config->segmentation_uma_name = kIosModuleRankerUmaName;
  config->AddSegmentId(kSegmentId, serve_default_config
                                       ? std::make_unique<IosModuleRanker>()
                                       : nullptr);
  config->auto_execute_and_cache = false;
  return config;
}

IosModuleRanker::IosModuleRanker() : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
IosModuleRanker::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);

  // Set output config.
  writer.AddOutputConfigForMultiClassClassifier(kIosModuleLabels.begin(),
                                                kIosModuleLabels.size(),
                                                kIosModuleLabels.size(),
                                                /*threshold=*/-99999.0);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLMinutes, proto::TimeUnit::MINUTE);

  // Set features.
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());

  // Add freshness for all modules as custom input.
  writer.AddFromInputContext("most_visited_tiles_input",
                             kMostVisitedTilesFreshness);
  writer.AddFromInputContext("shortcuts_input", kShortcutsFreshness);
  writer.AddFromInputContext("safety_check_input", kSafetyCheckFreshness);
  writer.AddFromInputContext("tab_resumption_input", kTabResumptionFreshness);
  writer.AddFromInputContext("parcel_tracking_input", kParcelTrackingFreshness);

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void IosModuleRanker::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() !=
      kUMAFeatures.size() + kIosModuleInputContextKeys.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  // TODO(ritikagup) : Add logic.

  ModelProvider::Response response(kIosModuleLabels.size(), 0);
  // Default ranking
  response[0] = 5;  // Most Visited Tiles
  response[1] = 4;  // Shortcuts
  response[2] = 3;  // Safety Check
  response[3] = 2;  // Tab resumption
  response[4] = 1;  // Parcel Tracking

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}
}  // namespace segmentation_platform
