// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/segmentation_platform/embedder/default_model/android_home_module_ranker.h"

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

// Default parameters for AndroidHomeModuleRanker model.
constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_ANDROID_HOME_MODULE_RANKER;
constexpr int64_t kModelVersion = 3;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// Wait until we have 0 days of data.
constexpr int64_t kMinSignalCollectionLength = 0;
// Refresh the result every time.
constexpr int64_t kResultTTLDays = 7;

constexpr std::array<const char*, 3> kAndroidHomeModuleLabels = {
    kSingleTab, kPriceChange, kTabResumptionForAndroidHome};

// InputFeatures.

// Enum values for the MagicStack.Clank.NewTabPage|StartSurface.Module.Click and
// MagicStack.Clank.NewTabPage|StartSurface.Module.TopImpressionV2 histograms.
constexpr std::array<int32_t, 1> kEnumValueForSingleTab{/*SingleTab=*/0};

constexpr std::array<int32_t, 1> kEnumValueForPriceChange{/*PriceChange=*/1};

constexpr std::array<int32_t, 1> kEnumValueForTabResumption{
    /*TabResumption=*/2};

// Set UMA metrics to use as input.
constexpr std::array<MetadataWriter::UMAFeature, 24> kUMAFeatures = {
    // Tab Resumption Module
    // 0
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.Click",
        7,
        kEnumValueForSingleTab.data(),
        kEnumValueForSingleTab.size()),
    // 1
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.StartSurface.Module.Click",
        7,
        kEnumValueForSingleTab.data(),
        kEnumValueForSingleTab.size()),
    // 2
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2",
        7,
        kEnumValueForSingleTab.data(),
        kEnumValueForSingleTab.size()),
    // 3
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.StartSurface.Module.TopImpressionV2",
        7,
        kEnumValueForSingleTab.data(),
        kEnumValueForSingleTab.size()),
    // 4
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.Click",
        28,
        kEnumValueForSingleTab.data(),
        kEnumValueForSingleTab.size()),
    // 5
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.StartSurface.Module.Click",
        28,
        kEnumValueForSingleTab.data(),
        kEnumValueForSingleTab.size()),
    // 6
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2",
        28,
        kEnumValueForSingleTab.data(),
        kEnumValueForSingleTab.size()),
    // 7
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.StartSurface.Module.TopImpressionV2",
        28,
        kEnumValueForSingleTab.data(),
        kEnumValueForSingleTab.size()),
    // Price Change Module
    // 8
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.Click",
        7,
        kEnumValueForPriceChange.data(),
        kEnumValueForPriceChange.size()),
    // 9
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.StartSurface.Module.Click",
        7,
        kEnumValueForPriceChange.data(),
        kEnumValueForPriceChange.size()),
    // 10
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2",
        7,
        kEnumValueForPriceChange.data(),
        kEnumValueForPriceChange.size()),
    // 11
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.StartSurface.Module.TopImpressionV2",
        7,
        kEnumValueForPriceChange.data(),
        kEnumValueForPriceChange.size()),
    // 12
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.Click",
        28,
        kEnumValueForPriceChange.data(),
        kEnumValueForPriceChange.size()),
    // 13
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.StartSurface.Module.Click",
        28,
        kEnumValueForPriceChange.data(),
        kEnumValueForPriceChange.size()),
    // 14
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2",
        28,
        kEnumValueForPriceChange.data(),
        kEnumValueForPriceChange.size()),
    // 15
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.StartSurface.Module.TopImpressionV2",
        28,
        kEnumValueForPriceChange.data(),
        kEnumValueForPriceChange.size()),
    // Tab Resumption Module
    // 16
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.Click",
        7,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),
    // 17
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.StartSurface.Module.Click",
        7,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),
    // 18
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2",
        7,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),
    // 19
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.StartSurface.Module.TopImpressionV2",
        7,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),
    // 20
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.Click",
        28,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),
    // 21
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.StartSurface.Module.Click",
        28,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),
    // 22
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2",
        28,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),
    // 23
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "MagicStack.Clank.StartSurface.Module.TopImpressionV2",
        28,
        kEnumValueForTabResumption.data(),
        kEnumValueForTabResumption.size()),
};

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
  config->auto_execute_and_cache = true;
  return config;
}

AndroidHomeModuleRanker::AndroidHomeModuleRanker()
    : DefaultModelProvider(kSegmentId) {}

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
                                                /*threshold=*/0);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLDays, proto::TimeUnit::DAY);

  // Set features.
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void AndroidHomeModuleRanker::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  ModelProvider::Response response(kAndroidHomeModuleLabels.size(), 0);
  // Default ranking
  response[0] = 2;  // Single Tab
  response[1] = 3;  // Price Change
  response[2] = 1;  // Tab Resumption

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}
}  // namespace segmentation_platform
