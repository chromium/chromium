// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/optimization_target_segmentation_dummy.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

BASE_FEATURE(kSegmentationPlatformOptimizationTargetSegmentationDummy,
             "SegmentationPlatformOptimizationTargetSegmentationDummy",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {
using proto::SegmentId;

// Default parameters for OptimizationTargetSegmentationDummy model.
constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY;
constexpr int64_t kModelVersion = 1;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// Wait until we have 7 days of data.
constexpr int64_t kMinSignalCollectionLength = 7;
// Refresh the result every 7 days.
constexpr int64_t kResultTTLDays = 7;

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 1> kUMAFeatures = {
    // Total amount of times user action was recorded in last 14 days.
    MetadataWriter::UMAFeature::FromUserAction("UserActionName", 14),
};

}  // namespace

// static
std::unique_ptr<Config> OptimizationTargetSegmentationDummy::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          kSegmentationPlatformOptimizationTargetSegmentationDummy)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kOptimizationTargetSegmentationDummyKey;
  config->segmentation_uma_name = kOptimizationTargetSegmentationDummyUmaName;
  config->AddSegmentId(kSegmentId,
                       std::make_unique<OptimizationTargetSegmentationDummy>());
  config->auto_execute_and_cache = false;

  return config;
}

OptimizationTargetSegmentationDummy::OptimizationTargetSegmentationDummy()
    : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
OptimizationTargetSegmentationDummy::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);

  // Set output config.
  const char kNegativeLabel[] = "Not_OptimizationTargetSegmentationDummy";
  writer.AddOutputConfigForBinaryClassifier(
      0.5,
      /*positive_label=*/kOptimizationTargetSegmentationDummyUmaName,
      kNegativeLabel);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLDays, proto::TimeUnit::DAY);

  // Set input features.
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());

  // Set output features.
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size(),
                        /*is_output=*/true);

  // Set ondemand model with a trigger.
  writer.AddDelayTrigger(0);

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void OptimizationTargetSegmentationDummy::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  float result = 1;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
