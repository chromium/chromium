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
constexpr int64_t kModelVersion = 6;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// Wait until we have 0 days of data.
constexpr int64_t kMinSignalCollectionLength = 0;
// Refresh the result every time.
constexpr int64_t kResultTTLMinutes = 1;

constexpr LabelPair<IosModuleRanker::Label> kIosModuleLabels[] = {
    {IosModuleRanker::kLabelMostVisitedTiles, kMostVisitedTiles},
    {IosModuleRanker::kLabelShortcuts, kShortcuts},
    {IosModuleRanker::kLabelSafetyCheck, kSafetyCheck},
    {IosModuleRanker::kLabelTabResumption, kTabResumption},
    {IosModuleRanker::kLabelParcelTracking, kParcelTracking},
    {IosModuleRanker::kLabelShopCard, kShopCard}};

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
constexpr std::array<int32_t, 1> kEnumValueForShopCard{/*ShopCard=*/21};

// TODO(ritikagup) : Loop through all the modules for these features for better
// readability. Set UMA metrics to use as input.
constexpr FeaturePair<IosModuleRanker::Feature> kIosModuleRankerFeatures[] = {
    // Most Visited Tiles
    {IosModuleRanker::kFeatureMVTClick7Days,
     features::UMAEnum("IOS.MagicStack.Module.Click", 7, kEnumValueForMVT)},
    {IosModuleRanker::kFeatureMVTImpression7Days,
     features::UMAEnum("IOS.MagicStack.Module.TopImpression",
                       7,
                       kEnumValueForMVT)},

    // Shortcuts
    {IosModuleRanker::kFeatureShortcutsClick7Days,
     features::UMAEnum("IOS.MagicStack.Module.Click",
                       7,
                       kEnumValueForShortcuts)},
    {IosModuleRanker::kFeatureShortcutsImpression7Days,
     features::UMAEnum("IOS.MagicStack.Module.TopImpression",
                       7,
                       kEnumValueForShortcuts)},

    // Safety Check
    {IosModuleRanker::kFeatureSafetyCheckClick7Days,
     features::UMAEnum("IOS.MagicStack.Module.Click",
                       7,
                       kEnumValueForSafetyCheck)},
    {IosModuleRanker::kFeatureSafetyCheckImpression7Days,
     features::UMAEnum("IOS.MagicStack.Module.TopImpression",
                       7,
                       kEnumValueForSafetyCheck)},

    // Most Visited Tiles
    {IosModuleRanker::kFeatureMVTClick28Days,
     features::UMAEnum("IOS.MagicStack.Module.Click", 28, kEnumValueForMVT)},
    {IosModuleRanker::kFeatureMVTImpression28Days,
     features::UMAEnum("IOS.MagicStack.Module.TopImpression",
                       28,
                       kEnumValueForMVT)},

    // Shortcuts
    {IosModuleRanker::kFeatureShortcutsClick28Days,
     features::UMAEnum("IOS.MagicStack.Module.Click",
                       28,
                       kEnumValueForShortcuts)},
    {IosModuleRanker::kFeatureShortcutsImpression28Days,
     features::UMAEnum("IOS.MagicStack.Module.TopImpression",
                       28,
                       kEnumValueForShortcuts)},

    // Safety Check
    {IosModuleRanker::kFeatureSafetyCheckClick28Days,
     features::UMAEnum("IOS.MagicStack.Module.Click",
                       28,
                       kEnumValueForSafetyCheck)},
    {IosModuleRanker::kFeatureSafetyCheckImpression28Days,
     features::UMAEnum("IOS.MagicStack.Module.TopImpression",
                       28,
                       kEnumValueForSafetyCheck)},

    {IosModuleRanker::kFeatureOpenMVT7Days,
     features::UserAction("MobileOmniboxShortcutsOpenMostVisitedItem", 7)},
    {IosModuleRanker::kFeatureOpenMVT28Days,
     features::UserAction("MobileOmniboxShortcutsOpenMostVisitedItem", 28)},
    {IosModuleRanker::kFeatureBookmarkManager7Days,
     features::UserAction("MobileBookmarkManagerEntryOpened", 7)},
    {IosModuleRanker::kFeatureBookmarkManager28Days,
     features::UserAction("MobileBookmarkManagerEntryOpened", 28)},
    {IosModuleRanker::kFeatureReadingList7Days,
     features::UserAction("MobileOmniboxShortcutsOpenReadingList", 7)},
    {IosModuleRanker::kFeatureReadingList28Days,
     features::UserAction("MobileOmniboxShortcutsOpenReadingList", 28)},
    {IosModuleRanker::kFeatureMobileReadingListOpen7Days,
     features::UserAction("MobileReadingListOpen", 7)},
    {IosModuleRanker::kFeatureMobileReadingListOpen28Days,
     features::UserAction("MobileReadingListOpen", 28)},
    {IosModuleRanker::kFeatureMobileReadingListAdd7Days,
     features::UserAction("MobileReadingListAdd", 7)},
    {IosModuleRanker::kFeatureMobileReadingListAdd28Days,
     features::UserAction("MobileReadingListAdd", 28)},

    // Tab Resumption
    {IosModuleRanker::kFeatureTabResumptionClick7Days,
     features::UMAEnum("IOS.MagicStack.Module.Click",
                       7,
                       kEnumValueForTabResumption)},
    {IosModuleRanker::kFeatureTabResumptionImpression7Days,
     features::UMAEnum("IOS.MagicStack.Module.TopImpression",
                       7,
                       kEnumValueForTabResumption)},
    {IosModuleRanker::kFeatureTabResumptionClick28Days,
     features::UMAEnum("IOS.MagicStack.Module.Click",
                       28,
                       kEnumValueForTabResumption)},
    {IosModuleRanker::kFeatureTabResumptionImpression28Days,
     features::UMAEnum("IOS.MagicStack.Module.TopImpression",
                       28,
                       kEnumValueForTabResumption)},

    // Parcel Tracking
    {IosModuleRanker::kFeatureParcelTrackingClick7Days,
     features::UMAEnum("IOS.MagicStack.Module.Click",
                       7,
                       kEnumValueForParcelTracking)},
    {IosModuleRanker::kFeatureParcelTrackingImpression7Days,
     features::UMAEnum("IOS.MagicStack.Module.TopImpression",
                       7,
                       kEnumValueForParcelTracking)},
    {IosModuleRanker::kFeatureParcelTrackingClick28Days,
     features::UMAEnum("IOS.MagicStack.Module.Click",
                       28,
                       kEnumValueForParcelTracking)},
    {IosModuleRanker::kFeatureParcelTrackingImpression28Days,
     features::UMAEnum("IOS.MagicStack.Module.TopImpression",
                       28,
                       kEnumValueForParcelTracking)},

    // ShopCard
    {IosModuleRanker::kFeatureShopCardClick7Days,
     features::UMAEnum("IOS.MagicStack.Module.Click",
                       7,
                       kEnumValueForShopCard)},
    {IosModuleRanker::kFeatureShopCardImpression7Days,
     features::UMAEnum("IOS.MagicStack.Module.TopImpression",
                       7,
                       kEnumValueForShopCard)},
    {IosModuleRanker::kFeatureShopCardClick28Days,
     features::UMAEnum("IOS.MagicStack.Module.Click",
                       28,
                       kEnumValueForShopCard)},
    {IosModuleRanker::kFeatureShopCardImpression28Days,
     features::UMAEnum("IOS.MagicStack.Module.TopImpression",
                       28,
                       kEnumValueForShopCard)},
    {IosModuleRanker::kFeatureMostVisitedTilesFreshness,
     features::InputContext(kMostVisitedTilesFreshness)},
    {IosModuleRanker::kFeatureShortcutsFreshness,
     features::InputContext(kShortcutsFreshness)},
    {IosModuleRanker::kFeatureSafetyCheckFreshness,
     features::InputContext(kSafetyCheckFreshness)},
    {IosModuleRanker::kFeatureTabResumptionFreshness,
     features::InputContext(kTabResumptionFreshness)},
    {IosModuleRanker::kFeatureParcelTrackingFreshness,
     features::InputContext(kParcelTrackingFreshness)},
    {IosModuleRanker::kFeatureShopCardFreshness,
     features::InputContext(kShopCardFreshness)},
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
  writer.AddOutputConfigForMultiClassClassifier<Label>(kIosModuleLabels,
                                                       /*threshold=*/-99999.0);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLMinutes, proto::TimeUnit::MINUTE);

  // Set features.
  writer.AddFeatures<Feature>(kIosModuleRankerFeatures);

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
  if (inputs.size() != kFeatureCount) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  // Most Visited Tiles score calculation.
  float mvt_weights[3] = {3.0, -0.3, 1.5};
  float mvt_engagement = inputs[kFeatureMVTClick28Days];
  float mvt_impression = inputs[kFeatureMVTImpression28Days];
  float mvt_freshness =
      TransformFreshness(inputs[kFeatureMostVisitedTilesFreshness], 1.0);
  float mvt_score = mvt_weights[0] * mvt_engagement +
                    mvt_weights[1] * mvt_impression +
                    mvt_weights[2] * mvt_freshness;

  // Shortcuts score calculation.
  float shortcuts_weights[3] = {1.5, -1.0, 2.0};
  float shortcuts_engagement = inputs[kFeatureShortcutsClick28Days];
  float shortcuts_impression = inputs[kFeatureShortcutsImpression28Days];
  float shortcuts_freshness =
      TransformFreshness(inputs[kFeatureShortcutsFreshness], 1.0);
  float shortcuts_score = shortcuts_weights[0] * shortcuts_engagement +
                          shortcuts_weights[1] * shortcuts_impression +
                          shortcuts_weights[2] * shortcuts_freshness;

  // Safety Check score calculation.
  float safety_check_weights[3] = {4.0, -12.0, 6.0};
  float safety_check_engagement = inputs[kFeatureSafetyCheckClick28Days];
  float safety_check_impression = inputs[kFeatureSafetyCheckImpression28Days];
  float safety_check_freshness =
      TransformFreshness(inputs[kFeatureSafetyCheckFreshness], 3.0);
  float safety_check_score = safety_check_weights[0] * safety_check_engagement +
                             safety_check_weights[1] * safety_check_impression +
                             safety_check_weights[2] * safety_check_freshness;

  // Tab Resumption score calculation.
  float tab_resumption_weights[3] = {1.5, -0.5, 1.0};
  float tab_resumption_engagement = inputs[kFeatureTabResumptionClick28Days];
  float tab_resumption_impression =
      inputs[kFeatureTabResumptionImpression28Days];
  float tab_resumption_freshness =
      TransformFreshness(inputs[kFeatureTabResumptionFreshness], 1.0);
  float tab_resumption_score =
      tab_resumption_weights[0] * tab_resumption_engagement +
      tab_resumption_weights[1] * tab_resumption_impression +
      tab_resumption_weights[2] * tab_resumption_freshness;

  // Parcel Tracking score calculation.
  float parcel_tracking_weights[3] = {6.0, -7.0, 7.0};
  float parcel_tracking_engagement = inputs[kFeatureParcelTrackingClick28Days];
  float parcel_tracking_impression =
      inputs[kFeatureParcelTrackingImpression28Days];
  float parcel_tracking_freshness =
      TransformFreshness(inputs[kFeatureParcelTrackingFreshness], 1.0);
  float parcel_tracking_score =
      parcel_tracking_weights[0] * parcel_tracking_engagement +
      parcel_tracking_weights[1] * parcel_tracking_impression +
      parcel_tracking_weights[2] * parcel_tracking_freshness;

  // ShopCard score calculation. x = 29
  float shop_card_weights[3] = {6.0, -5.0, 5.0};
  float shop_card_engagement = inputs[kFeatureShopCardClick28Days];
  float shop_card_impression = inputs[kFeatureShopCardImpression28Days];
  float shop_card_freshness =
      TransformFreshness(inputs[kFeatureShopCardFreshness], 1.0);
  float shop_card_score = shop_card_weights[0] * shop_card_engagement +
                          shop_card_weights[1] * shop_card_impression +
                          shop_card_weights[2] * shop_card_freshness;

  ModelProvider::Response response(kLabelCount, 0);
  // Default ranking
  response[kLabelMostVisitedTiles] = mvt_score;
  response[kLabelShortcuts] = shortcuts_score;
  response[kLabelSafetyCheck] = safety_check_score;
  response[kLabelTabResumption] = tab_resumption_score;
  response[kLabelParcelTracking] = parcel_tracking_score;
  response[kLabelShopCard] = shop_card_score;

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
  writer.AddOutputConfigForMultiClassClassifier<IosModuleRanker::Label>(
      kIosModuleLabels,
      /*threshold=*/-99999.0);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLMinutes, proto::TimeUnit::MINUTE);

  // Set features.
  writer.AddFeatures<IosModuleRanker::Feature>(kIosModuleRankerFeatures);

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void TestIosModuleRanker::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != IosModuleRanker::kFeatureCount) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  ModelProvider::Response response(IosModuleRanker::kLabelCount, 0);
  std::string card_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "test-ios-module-ranker");
  if (card_type == "mvt") {
    response[IosModuleRanker::kLabelMostVisitedTiles] = 6;
  } else if (card_type == "shortcut") {
    response[IosModuleRanker::kLabelShortcuts] = 6;
  } else if (card_type == "safety_check") {
    response[IosModuleRanker::kLabelSafetyCheck] = 6;
  } else if (card_type == "tab_resumption") {
    response[IosModuleRanker::kLabelTabResumption] = 6;
  } else if (card_type == "parcel_tracking") {
    response[IosModuleRanker::kLabelParcelTracking] = 6;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

}  // namespace segmentation_platform
