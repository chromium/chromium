// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/url_visit_resumption_ranker.h"

#include <memory>

#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/visited_url_ranking/public/url_visit_schema.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_URL_VISIT_RESUMPTION_RANKER;
constexpr uint64_t kRankerVersion = 2;

constexpr std::array<MetadataWriter::UMAFeature, 1> kOutput = {
    MetadataWriter::UMAFeature::FromUserAction("MetadataWriter", 1)};

}  // namespace

// static
std::unique_ptr<Config> URLVisitResumptionRanker::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformURLVisitResumptionRanker)) {
    return nullptr;
  }

  auto config = std::make_unique<Config>();
  config->segmentation_key = kURLVisitResumptionRankerKey;
  config->segmentation_uma_name = kURLVisitResumptionRankerUmaName;
  config->AddSegmentId(kSegmentId,
                       std::make_unique<URLVisitResumptionRanker>());
  config->auto_execute_and_cache = false;
  return config;
}

URLVisitResumptionRanker::URLVisitResumptionRanker()
    : DefaultModelProvider(kSegmentId),
      use_random_score_(base::GetFieldTrialParamByFeatureAsBool(
          features::kSegmentationPlatformURLVisitResumptionRanker,
          "use_random_score",
          false)) {}

URLVisitResumptionRanker::~URLVisitResumptionRanker() = default;

std::unique_ptr<DefaultModelProvider::ModelConfig>
URLVisitResumptionRanker::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      /*min_signal_collection_length_days=*/0,
      /*signal_storage_length_days=*/28);

  // Set features.
  for (const auto& field : visited_url_ranking::kURLVisitAggregateSchema) {
    writer.AddCustomInput(MetadataWriter::CustomInput{
        .tensor_length = 1,
        .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
        .name = field.name});
  }

  metadata.mutable_output_config()
      ->mutable_predictor()
      ->mutable_generic_predictor()
      ->add_output_labels(kURLVisitResumptionRankerKey);

  metadata.set_upload_tensors(true);
  auto* outputs = metadata.mutable_training_outputs();
  outputs->mutable_trigger_config()->set_decision_type(
      proto::TrainingOutputs::TriggerConfig::ONDEMAND);

  writer.AddUmaFeatures(kOutput.data(), kOutput.size(), /*is_output=*/true);
  return std::make_unique<ModelConfig>(std::move(metadata), kRankerVersion);
}

void URLVisitResumptionRanker::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  float time_since_modified_sec =
      inputs[visited_url_ranking::URLVisitAggregateRankingModelInputSignals::
                 kTimeSinceLastModifiedSec];
  float resumption_score = 0;
  if (use_random_score_) {
    resumption_score = base::RandFloat();
  } else {
    if (time_since_modified_sec < 0) {
      resumption_score = 0;
    } else if (time_since_modified_sec == 0) {
      resumption_score = 1;
    } else {
      resumption_score = 1.0f / time_since_modified_sec;
    }
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                ModelProvider::Response(1, resumption_score)));
}

}  // namespace segmentation_platform
