// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/tab_resumption_ranker.h"

#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/embedder/input_delegate/tab_session_source.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

SegmentId kSegmentId = SegmentId::TAB_RESUMPTION_CLASSIFIER;
constexpr uint64_t kTabResumptionRankerVersion = 1;

}  // namespace

// static
std::unique_ptr<Config> TabResumptionRanker::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformTabResumptionRanker)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kTabResumptionClassifierKey;
  config->segmentation_uma_name = kTabResumptionClassifierUmaName;
  config->AddSegmentId(kSegmentId, std::make_unique<TabResumptionRanker>());
  config->auto_execute_and_cache = false;
  return config;
}

TabResumptionRanker::TabResumptionRanker()
    : DefaultModelProvider(SegmentId::TAB_RESUMPTION_CLASSIFIER) {}
TabResumptionRanker::~TabResumptionRanker() = default;

std::unique_ptr<DefaultModelProvider::ModelConfig>
TabResumptionRanker::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      /*min_signal_collection_length_days=*/0,
      /*signal_storage_length_days=*/0);

  // Set features.
  writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = processing::TabSessionSource::kNumInputs,
      .fill_policy = proto::CustomInput::FILL_TAB_METRICS,
      .name = "tab"});

  metadata.mutable_output_config()
      ->mutable_predictor()
      ->mutable_generic_predictor()
      ->add_output_labels(kTabResumptionClassifierKey);

  return std::make_unique<ModelConfig>(std::move(metadata),
                                       kTabResumptionRankerVersion);
}

void TabResumptionRanker::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != processing::TabSessionSource::kNumInputs) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  // Assumes the first input to the model is TAB_METRICS.
  float time_since_modified_sec =
      inputs[processing::TabSessionSource::kInputTimeSinceModifiedSec];
  if (time_since_modified_sec == 0) {
    time_since_modified_sec =
        inputs[processing::TabSessionSource::kInputLocalTabTimeSinceModified];
  }
  // Add 1 to avoid divide by 0.
  float resumption_score = 1.0 / (time_since_modified_sec + 1);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                ModelProvider::Response(1, resumption_score)));
}

}  // namespace segmentation_platform
