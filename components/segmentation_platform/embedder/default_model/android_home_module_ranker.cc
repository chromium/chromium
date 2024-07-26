// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/segmentation_platform/embedder/default_model/android_home_module_ranker.h"

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

// Default parameters for AndroidHomeModuleRanker model.
constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_ANDROID_HOME_MODULE_RANKER;
constexpr int64_t kModelVersion = 6;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// Wait until we have 0 days of data.
constexpr int64_t kMinSignalCollectionLength = 0;
// Refresh the result every time.
constexpr int64_t kResultTTLMinutes = 5;

constexpr std::array<const char*, 4> kAndroidHomeModuleLabels = {
    kPriceChange, kSingleTab, kTabResumptionForAndroidHome, kSafetyHub};

constexpr std::array<const char*, 4> kAndroidHomeModuleInputContextKeys = {
    kPriceChangeFreshness, kSingleTabFreshness,
    kTabResumptionForAndroidHomeFreshness, kSafetyHubFreshness};

// InputFeatures.

// Enum values for the MagicStack.Clank.NewTabPage|StartSurface.Module.Click and
// MagicStack.Clank.NewTabPage|StartSurface.Module.TopImpressionV2 histograms.
constexpr std::array<int32_t, 1> kEnumValueForSingleTab{/*SingleTab=*/0};

constexpr std::array<int32_t, 1> kEnumValueForPriceChange{/*PriceChange=*/1};

constexpr std::array<int32_t, 1> kEnumValueForTabResumption{
    /*TabResumption=*/2};

constexpr std::array<int32_t, 1> kEnumValueForSafetyHub{/*SafetyHub=*/3};

// Set UMA metrics to use as input.
constexpr std::array<MetadataWriter::UMAFeature, 8> kUMAFeatures = {
    // Single Tab Module
    // 0 : click
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.Click",
        28,
        kEnumValueForSingleTab.data(),
        kEnumValueForSingleTab.size()),
    // 1 : impression
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2",
        28,
        kEnumValueForSingleTab.data(),
        kEnumValueForSingleTab.size()),
    // Price Change Module
    // 2 : click
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.Click",
        28,
        kEnumValueForPriceChange.data(),
        kEnumValueForPriceChange.size()),
    // 3 : impression
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2",
        28,
        kEnumValueForPriceChange.data(),
        kEnumValueForPriceChange.size()),
    // Tab Resumption Module
    // 4 : click
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.Click",
        28,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),
    // 5 : impression
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2",
        28,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),
    // Safety Hub Module
    // 6 : click
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.Click",
        28,
        kEnumValueForSafetyHub.data(),
        kEnumValueForSafetyHub.size()),
    // 7 : impression
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2",
        28,
        kEnumValueForSafetyHub.data(),
        kEnumValueForSafetyHub.size()),
};

float TransformFreshness(float freshness_score, float freshness_threshold) {
  float new_freshness_score = 0.0;
  if (freshness_score >= 0.0 and freshness_score <= freshness_threshold) {
    new_freshness_score = 1.0;
  }
  return new_freshness_score;
}

}  // namespace

// static
std::unique_ptr<Config> AndroidHomeModuleRanker::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformAndroidHomeModuleRanker)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kAndroidHomeModuleRankerKey;
  config->segmentation_uma_name = kAndroidHomeModuleRankerUmaName;
  config->AddSegmentId(kSegmentId, std::make_unique<AndroidHomeModuleRanker>());
  config->auto_execute_and_cache = !base::FeatureList::IsEnabled(
      features::kSegmentationPlatformAndroidHomeModuleRankerV2);
  return config;
}

AndroidHomeModuleRanker::AndroidHomeModuleRanker()
    : DefaultModelProvider(kSegmentId),
      is_android_home_module_ranker_v2_enabled(base::FeatureList::IsEnabled(
          features::kSegmentationPlatformAndroidHomeModuleRankerV2)) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
AndroidHomeModuleRanker::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);
  metadata.set_upload_tensors(true);

  // Set output config.
  writer.AddOutputConfigForMultiClassClassifier(kAndroidHomeModuleLabels,
                                                kAndroidHomeModuleLabels.size(),
                                                /*threshold=*/-99999.0);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLMinutes, proto::TimeUnit::MINUTE);

  writer.SetIgnorePreviousModelTTLInOutputConfig();

  // Set features.
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());

  if (is_android_home_module_ranker_v2_enabled) {
    // Add freshness for all modules as custom input.
    writer.AddFromInputContext("single_tab_input", kSingleTabFreshness);
    writer.AddFromInputContext("price_change_input", kPriceChangeFreshness);
    writer.AddFromInputContext("tab_resumption_input",
                               kTabResumptionForAndroidHomeFreshness);
    writer.AddFromInputContext("safety_hub_input", kSafetyHubFreshness);
  }

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void AndroidHomeModuleRanker::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  size_t expected_input_size =
      is_android_home_module_ranker_v2_enabled
          ? kUMAFeatures.size() + kAndroidHomeModuleInputContextKeys.size()
          : kUMAFeatures.size();
  if (inputs.size() != expected_input_size) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  // Add logic here.
  // Single Tab score calculation.
  float single_tab_weights[3] = {1.5, -0.5, 1.0};
  float single_tab_engagement = inputs[0];
  float single_tab_impression = inputs[1];
  float single_tab_freshness = is_android_home_module_ranker_v2_enabled
                                   ? TransformFreshness(inputs[8], 1.0)
                                   : 0.0;
  float single_tab_score = single_tab_weights[0] * single_tab_engagement +
                           single_tab_weights[1] * single_tab_impression +
                           single_tab_weights[2] * single_tab_freshness;

  // Price Change score calculation.
  float price_change_weights[3] = {2.0, -1.0, 2.0};
  float price_change_engagement = inputs[2];
  float price_change_impression = inputs[3];
  float price_change_freshness = is_android_home_module_ranker_v2_enabled
                                     ? TransformFreshness(inputs[9], 1.0)
                                     : 0.0;
  float price_change_score = price_change_weights[0] * price_change_engagement +
                             price_change_weights[1] * price_change_impression +
                             price_change_weights[2] * price_change_freshness;

  // Tab Resumption score calculation.
  float tab_resumption_weights[3] = {1.5, -0.5, 1.0};
  float tab_resumption_engagement = inputs[4];
  float tab_resumption_impression = inputs[5];
  float tab_resumption_freshness = is_android_home_module_ranker_v2_enabled
                                       ? TransformFreshness(inputs[10], 1.0)
                                       : 0.0;
  float tab_resumption_score =
      tab_resumption_weights[0] * tab_resumption_engagement +
      tab_resumption_weights[1] * tab_resumption_impression +
      tab_resumption_weights[2] * tab_resumption_freshness;

  // Safety Hub score calculation.
  float safety_hub_weights[3] = {2.5, -2, 2.5};
  float safety_hub_engagement = inputs[6];
  float safety_hub_impression = inputs[7];
  float safety_hub_freshness = is_android_home_module_ranker_v2_enabled
                                   ? TransformFreshness(inputs[11], 1.0)
                                   : 0.0;
  float safety_hub_score = safety_hub_weights[0] * safety_hub_engagement +
                           safety_hub_weights[1] * safety_hub_impression +
                           safety_hub_weights[2] * safety_hub_freshness;

  ModelProvider::Response response(kAndroidHomeModuleLabels.size(), 0);
  // Default ranking
  response[0] = price_change_score;    // Price Change
  response[1] = single_tab_score;      // Single tab
  response[2] = tab_resumption_score;  // Tab Resumption
  response[3] = safety_hub_score;      // Safety Hub

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}
}  // namespace segmentation_platform
