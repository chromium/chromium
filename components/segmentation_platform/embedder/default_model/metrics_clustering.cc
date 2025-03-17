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
constexpr int64_t kModelVersion = 2;
constexpr int64_t kSignalStorageLength = 28;
constexpr int64_t kMinSignalCollectionLength = 15;
constexpr int64_t kResultTTLHours = 1;

// InputFeatures.

// Set UMA metrics to use as input.
constexpr std::array<MetadataWriter::UMAFeature, 1> kUMAFeatures = {
    MetadataWriter::UMAFeature{
        .signal_type = proto::SignalType::HISTOGRAM_VALUE,
        .name = "Session.TotalDuration",
        .bucket_count = 14,
        .tensor_length = 14,
        .aggregation = proto::Aggregation::BUCKETED_COUNT,
        .enum_ids_size = 0},
};

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
  config->auto_execute_and_cache = false;
  return config;
}

MetricsClustering::MetricsClustering() : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
MetricsClustering::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);
  writer.AddOutputConfigForGenericPredictor({"active_count"});

  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLHours, proto::TimeUnit::HOUR);

  // Set features.
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());

  // Use a time from last day, since metrics related to the survey opening
  // should not be included in the data.
  base::Time prediction_time = base::Time::Now() - base::Days(1);
  metadata.set_fixed_prediction_timestamp(
      prediction_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void MetricsClustering::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != 14) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  int day_count = 0;
  for (unsigned i = 0; i < 14; ++i) {
    day_count += inputs[i] > 0 ? 1 : 0;
  }

  if (day_count == 0) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  std::vector<float> result;
  result.push_back(day_count);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

}  // namespace segmentation_platform
