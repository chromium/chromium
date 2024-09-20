// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/metrics_clustering.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for MetricsClustering model.
constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_METRICS_CLUSTERING;
constexpr int64_t kModelVersion = 1;
constexpr int64_t kSignalStorageLength = 28;
constexpr int64_t kMinSignalCollectionLength = 28;
constexpr int64_t kResultTTLDays = 90;

// InputFeatures.

// Set UMA metrics to use as input.
// TODO(b/355993452): Fill in the necessary signals for prediction.
constexpr std::array<MetadataWriter::UMAFeature, 0> kUMAFeatures = {};

}  // namespace

// static
std::unique_ptr<Config> MetricsClustering::GetConfig() {
  if (!base::FeatureList::IsEnabled(features::kSegmentationSurveyPage)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kMetricsClusteringKey;
  config->segmentation_uma_name = kMetricsClusteringUmaName;
  config->AddSegmentId(kSegmentId, std::make_unique<MetricsClustering>());
  config->auto_execute_and_cache = true;
  return config;
}

MetricsClustering::MetricsClustering() : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
MetricsClustering::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);
  writer.AddOutputConfigForGenericPredictor({"label1", "label2", "label3"});

  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLDays, proto::TimeUnit::DAY);

  // Set features.
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void MetricsClustering::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  // This model will not be used, server will set up the right values. Log error
  // in case we reach here.
  std::vector<float> result{-1, -1, -1};

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

}  // namespace segmentation_platform
