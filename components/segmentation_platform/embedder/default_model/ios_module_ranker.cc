// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/ios_module_ranker.h"

#include <memory>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/metrics_hashes.h"
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
// Default parameters for TestIosModuleRanker model.
constexpr SegmentId kTestSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_IOS_MODULE_RANKER_TEST;
constexpr int64_t kModelVersion = 5;
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

// Output features:

constexpr char kClickHistogram[] = "IOS.MagicStack.Module.Click";

constexpr std::array<float, 1> kOutputFeatureDefaultValue{100};
constexpr std::array<MetadataWriter::UMAFeature, 1> kOutputFeatures = {
    MetadataWriter::UMAFeature::FromValueHistogram(
        kClickHistogram,
        1,
        proto::Aggregation::LATEST_OR_DEFAULT,
        kOutputFeatureDefaultValue.size(),
        kOutputFeatureDefaultValue.data())};

// InputFeatures.

// Enum values for the IOS.MagicStack.Module.Click and
// IOS.MagicStack.Module.TopImpression histograms.
constexpr std::array<int32_t, 1> kEnumValueForMVT{/*MostVisitedTiles=*/0};
constexpr std::array<int32_t, 1> kEnumValueForShortcuts{/*Shortcuts=*/1};
// TODO(ritikagup) : Update this if needed once histogram is available.
constexpr std::array<int32_t, 1> kEnumValueForSafetyCheck{/*SafetyCheck=*/7};
constexpr std::array<int32_t, 1> kEnumValueForTabResumption{
    /*TabResumption=*/10};
constexpr std::array<int32_t, 1> kEnumValueForParcelTracking{
    /*ParcelTracking=*/11};

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

float TransformFreshness(float freshness_score, float freshness_threshold) {
  float new_freshness_score = 0.0;
  if (freshness_score >= 0.0 and freshness_score <= freshness_threshold) {
    new_freshness_score = 1.0;
  } else if (freshness_score == -1.0) {
    new_freshness_score = 0.0;
  }
  return new_freshness_score;
}

}  // namespace

// static
std::unique_ptr<Config> IosModuleRanker::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformIosModuleRanker)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kIosModuleRankerKey;
  config->segmentation_uma_name = kIosModuleRankerUmaName;
  config->AddSegmentId(kSegmentId, std::make_unique<IosModuleRanker>());
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
  metadata.set_upload_tensors(true);

  // Set output config.
  writer.AddOutputConfigForMultiClassClassifier(kIosModuleLabels,
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

  if (base::GetFieldTrialParamByFeatureAsBool(
          features::kSegmentationPlatformIosModuleRanker, "add-trigger-config",
          false)) {
    writer.AddUmaFeatures(kOutputFeatures.data(), kOutputFeatures.size(),
                          /*is_output=*/true);

    auto* outputs = metadata.mutable_training_outputs();
    auto* uma_trigger =
        outputs->mutable_trigger_config()->add_observation_trigger();
    auto* uma_feature =
        uma_trigger->mutable_uma_trigger()->mutable_uma_feature();
    uma_feature->set_name(kClickHistogram);
    uma_feature->set_name_hash(base::HashMetricName(kClickHistogram));
    uma_feature->set_type(proto::SignalType::HISTOGRAM_ENUM);
    outputs->mutable_trigger_config()->set_decision_type(
        proto::TrainingOutputs::TriggerConfig::ONDEMAND);
  }

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void IosModuleRanker::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() !=
      kUMAFeatures.size() + kIosModuleInputContextKeys.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  // Most Visited Tiles score calculation.
  float mvt_weights[3] = {3.0, -0.3, 1.5};
  float mvt_engagement = inputs[6];
  float mvt_impression = inputs[7];
  float mvt_freshness = TransformFreshness(inputs[30], 1.0);
  float mvt_score = mvt_weights[0] * mvt_engagement +
                    mvt_weights[1] * mvt_impression +
                    mvt_weights[2] * mvt_freshness;

  // Shortcuts score calculation.
  float shortcuts_weights[3] = {1.5, -1.0, 2.0};
  float shortcuts_engagement = inputs[8];
  float shortcuts_impression = inputs[9];
  float shortcuts_freshness = TransformFreshness(inputs[31], 1.0);
  float shortcuts_score = shortcuts_weights[0] * shortcuts_engagement +
                          shortcuts_weights[1] * shortcuts_impression +
                          shortcuts_weights[2] * shortcuts_freshness;

  // Safety Check score calculation.
  float safety_check_weights[3] = {4.0, -12.0, 6.0};
  float safety_check_engagement = inputs[10];
  float safety_check_impression = inputs[11];
  float safety_check_freshness = TransformFreshness(inputs[32], 3.0);
  float safety_check_score = safety_check_weights[0] * safety_check_engagement +
                             safety_check_weights[1] * safety_check_impression +
                             safety_check_weights[2] * safety_check_freshness;

  // Tab Resumption score calculation.
  float tab_resumption_weights[3] = {1.5, -0.5, 1.0};
  float tab_resumption_engagement = inputs[24];
  float tab_resumption_impression = inputs[25];
  float tab_resumption_freshness = TransformFreshness(inputs[33], 1.0);
  float tab_resumption_score =
      tab_resumption_weights[0] * tab_resumption_engagement +
      tab_resumption_weights[1] * tab_resumption_impression +
      tab_resumption_weights[2] * tab_resumption_freshness;

  // Parcel Tracking score calculation.
  float parcel_tracking_weights[3] = {6.0, -7.0, 7.0};
  float parcel_tracking_engagement = inputs[28];
  float parcel_tracking_impression = inputs[29];
  float parcel_tracking_freshness = TransformFreshness(inputs[34], 1.0);
  float parcel_tracking_score =
      parcel_tracking_weights[0] * parcel_tracking_engagement +
      parcel_tracking_weights[1] * parcel_tracking_impression +
      parcel_tracking_weights[2] * parcel_tracking_freshness;

  ModelProvider::Response response(kIosModuleLabels.size(), 0);
  // Default ranking
  response[0] = mvt_score;              // Most Visited Tiles
  response[1] = shortcuts_score;        // Shortcuts
  response[2] = safety_check_score;     // Safety Check
  response[3] = tab_resumption_score;   // Tab resumption
  response[4] = parcel_tracking_score;  // Parcel Tracking

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

// static
std::unique_ptr<Config> TestIosModuleRanker::GetConfig() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kIosModuleRankerKey;
  config->segmentation_uma_name = kIosModuleRankerUmaName;
  config->AddSegmentId(kTestSegmentId, std::make_unique<TestIosModuleRanker>());
  config->auto_execute_and_cache = false;
  return config;
}

TestIosModuleRanker::TestIosModuleRanker()
    : DefaultModelProvider(kTestSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
TestIosModuleRanker::GetModelConfig() {
  // TODO(b/331914464): Merge this duplicate implementation with
  // IosModuleRanker's.
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);
  metadata.set_upload_tensors(true);

  // Set output config.
  writer.AddOutputConfigForMultiClassClassifier(kIosModuleLabels,
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

void TestIosModuleRanker::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() !=
      kUMAFeatures.size() + kIosModuleInputContextKeys.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  ModelProvider::Response response(kIosModuleLabels.size(), 0);
  std::string card_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "test-ios-module-ranker");
  if (card_type == "mvt") {
    response[0] = 6;
  } else if (card_type == "shortcut") {
    response[1] = 6;
  } else if (card_type == "safety_check") {
    response[2] = 6;
  } else if (card_type == "tab_resumption") {
    response[3] = 6;
  } else if (card_type == "parcel_tracking") {
    response[4] = 6;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

}  // namespace segmentation_platform
