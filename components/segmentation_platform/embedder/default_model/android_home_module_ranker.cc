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
constexpr int64_t kModelVersion = 9;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// Wait until we have 0 days of data.
constexpr int64_t kMinSignalCollectionLength = 0;
// Refresh the result every time.
constexpr int64_t kResultTTLMinutes = 5;

#define ANDROID_HOME_MODULE_LABELS(F) \
  F(kPriceChangeIdx, kPriceChange)    \
  F(kSingleTabIdx, kSingleTab)        \
  F(kSafetyHubIdx, kSafetyHub)

SEGMENTATION_DEFINE_LABELS(kAndroidHomeModuleLabels,
                           ANDROID_HOME_MODULE_LABELS);

// Enum values for the MagicStack.Clank.NewTabPage|StartSurface.Module.Click and
// MagicStack.Clank.NewTabPage|StartSurface.Module.TopImpressionV2 histograms.
constexpr std::array<int32_t, 1> kEnumValueForSingleTab{/*SingleTab=*/0};

constexpr std::array<int32_t, 1> kEnumValueForPriceChange{/*PriceChange=*/1};

constexpr std::array<int32_t, 1> kEnumValueForSafetyHub{/*SafetyHub=*/3};

#define ANDROID_HOME_MODULE_UMA_FEATURES(F)                                  \
  F(kSingleTabClickIdx,                                                      \
    SEGMENTATION_UMA_ENUM("MagicStack.Clank.NewTabPage.Module.Click", 28,    \
                          kEnumValueForSingleTab.data(),                     \
                          kEnumValueForSingleTab.size()))                    \
  F(kSingleTabImpressionIdx,                                                 \
    SEGMENTATION_UMA_ENUM(                                                   \
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2", 28,            \
        kEnumValueForSingleTab.data(), kEnumValueForSingleTab.size()))       \
  F(kPriceChangeClickIdx,                                                    \
    SEGMENTATION_UMA_ENUM("MagicStack.Clank.NewTabPage.Module.Click", 28,    \
                          kEnumValueForPriceChange.data(),                   \
                          kEnumValueForPriceChange.size()))                  \
  F(kPriceChangeImpressionIdx,                                               \
    SEGMENTATION_UMA_ENUM(                                                   \
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2", 28,            \
        kEnumValueForPriceChange.data(), kEnumValueForPriceChange.size()))   \
  F(kSafetyHubClickIdx,                                                      \
    SEGMENTATION_UMA_ENUM("MagicStack.Clank.NewTabPage.Module.Click", 28,    \
                          kEnumValueForSafetyHub.data(),                     \
                          kEnumValueForSafetyHub.size()))                    \
  F(kSafetyHubImpressionIdx,                                                 \
    SEGMENTATION_UMA_ENUM(                                                   \
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2", 28,            \
        kEnumValueForSafetyHub.data(), kEnumValueForSafetyHub.size()))       \
  F(kSingleTabFreshnessIdx, SEGMENTATION_INPUT_CONTEXT(kSingleTabFreshness)) \
  F(kPriceChangeFreshnessIdx,                                                \
    SEGMENTATION_INPUT_CONTEXT(kPriceChangeFreshness))                       \
  F(kSafetyHubFreshnessIdx, SEGMENTATION_INPUT_CONTEXT(kSafetyHubFreshness))

SEGMENTATION_DEFINE_FEATURES(kUMAFeatures, ANDROID_HOME_MODULE_UMA_FEATURES);

float TransformFreshness(float freshness_score, float freshness_threshold) {
  float new_freshness_score = 0.0;
  if (freshness_score >= 0.0 && freshness_score <= freshness_threshold) {
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
                                                kLabelsCount,
                                                /*threshold=*/-99999.0);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLMinutes, proto::TimeUnit::MINUTE);

  writer.SetIgnorePreviousModelTTLInOutputConfig();

  // Set features.
  if (is_android_home_module_ranker_v2_enabled) {
    writer.AddFeatures(kUMAFeatures);
  } else {
    // When V2 is disabled, only add the UMA features.
    base::span<const MetadataWriter::Feature> all_features(kUMAFeatures);
    writer.AddFeatures(all_features.subspan(
        0u, static_cast<unsigned>(kSingleTabFreshnessIdx)));
  }

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void AndroidHomeModuleRanker::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  size_t expected_input_size = is_android_home_module_ranker_v2_enabled
                                   ? kCount
                                   : kSingleTabFreshnessIdx;

  if (inputs.size() != expected_input_size) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  // Add logic here.
  // Single Tab score calculation.
  float single_tab_weights[3] = {1.5, -0.5, 1.0};
  float single_tab_engagement = inputs[kSingleTabClickIdx];
  float single_tab_impression = inputs[kSingleTabImpressionIdx];
  float single_tab_freshness =
      is_android_home_module_ranker_v2_enabled
          ? TransformFreshness(inputs[kSingleTabFreshnessIdx], 1.0)
          : 0.0;
  float single_tab_score = single_tab_weights[0] * single_tab_engagement +
                           single_tab_weights[1] * single_tab_impression +
                           single_tab_weights[2] * single_tab_freshness;

  // Price Change score calculation.
  float price_change_weights[3] = {2.0, -1.0, 2.0};
  float price_change_engagement = inputs[kPriceChangeClickIdx];
  float price_change_impression = inputs[kPriceChangeImpressionIdx];
  float price_change_freshness =
      is_android_home_module_ranker_v2_enabled
          ? TransformFreshness(inputs[kPriceChangeFreshnessIdx], 1.0)
          : 0.0;
  float price_change_score = price_change_weights[0] * price_change_engagement +
                             price_change_weights[1] * price_change_impression +
                             price_change_weights[2] * price_change_freshness;

  // Safety Hub score calculation.
  float safety_hub_weights[3] = {2.5, -2, 2.5};
  float safety_hub_engagement = inputs[kSafetyHubClickIdx];
  float safety_hub_impression = inputs[kSafetyHubImpressionIdx];
  float safety_hub_freshness =
      is_android_home_module_ranker_v2_enabled
          ? TransformFreshness(inputs[kSafetyHubFreshnessIdx], 1.0)
          : 0.0;
  float safety_hub_score = safety_hub_weights[0] * safety_hub_engagement +
                           safety_hub_weights[1] * safety_hub_impression +
                           safety_hub_weights[2] * safety_hub_freshness;

  ModelProvider::Response response(kLabelsCount, 0);
  // Default ranking
  response[kPriceChangeIdx] = price_change_score;
  response[kSingleTabIdx] = single_tab_score;
  response[kSafetyHubIdx] = safety_hub_score;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}
}  // namespace segmentation_platform
