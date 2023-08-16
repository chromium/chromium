// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/tab_resumption_ranker.h"

#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/embedder/input_delegate/tab_session_source.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

constexpr SegmentId kSegmentId = SegmentId::TAB_RESUMPTION_CLASSIFIER;
constexpr uint64_t kTabResumptionRankerVersion = 1;

// kPaintTiming_NavigationToFirstPaintNameHash.
constexpr const char kLoadCountSql[] =
    "SELECT COUNT(metrics.id) "
    "FROM metrics JOIN urls ON metrics.url_id=urls.url_id "
    "WHERE instr(urls.url,?)>0 AND metrics.metric_hash='64BD7CCE5A95BF00';";

// kPageTiming_TotalForegroundDurationNameHash.
constexpr const char kUsageDurationSql[] =
    "SELECT SUM(metrics.metric_value) "
    "FROM metrics JOIN urls ON metrics.url_id=urls.url_id "
    "WHERE instr(urls.url,?)>0 AND metrics.metric_hash='D7DB428ED4C5956A';";

constexpr std::array<UkmMetricHash, 2> kUkmMetrics{
    UkmMetricHash::FromUnsafeValue(
        ukm::builders::PageLoad::
            kPaintTiming_NavigationToFirstContentfulPaintNameHash),
    UkmMetricHash::FromUnsafeValue(
        ukm::builders::PageLoad::kPageTiming_TotalForegroundDurationNameHash),
};
constexpr std::array<MetadataWriter::SqlFeature::EventAndMetrics, 1> kSqlEvent{
    MetadataWriter::SqlFeature::EventAndMetrics{
        .event_hash = UkmEventHash::FromUnsafeValue(
            ukm::builders::PageLoad::kEntryNameHash),
        .metrics = kUkmMetrics.data(),
        .metrics_size = kUkmMetrics.size()},
};
constexpr MetadataWriter::SqlFeature kLoadCount{
    .sql = kLoadCountSql,
    .events = kSqlEvent.data(),
    .events_size = kSqlEvent.size()};

constexpr MetadataWriter::SqlFeature kUsageDuration{
    .sql = kUsageDurationSql,
    .events = kSqlEvent.data(),
    .events_size = kSqlEvent.size()};

constexpr std::array<MetadataWriter::CustomInput::Arg, 1> kBindValueArg{
    std::make_pair("name", "origin")};
constexpr MetadataWriter::CustomInput kBindValue{
    .tensor_length = 1,
    .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
    .arg = kBindValueArg.data(),
    .arg_size = kBindValueArg.size()};

constexpr std::array<MetadataWriter::UMAFeature, 1> kOutput = {
    MetadataWriter::UMAFeature::FromUserAction("MetadataWriter", 1)};

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
  writer.AddSqlFeature(
      kLoadCount,
      {std::make_pair(proto::SqlFeature_BindValue::STRING, kBindValue)});
  writer.AddSqlFeature(
      kUsageDuration,
      {std::make_pair(proto::SqlFeature_BindValue::STRING, kBindValue)});

  metadata.mutable_output_config()
      ->mutable_predictor()
      ->mutable_generic_predictor()
      ->add_output_labels(kTabResumptionClassifierKey);

  metadata.set_upload_tensors(true);
  auto* outputs = metadata.mutable_training_outputs();
  outputs->mutable_trigger_config()->set_decision_type(
      proto::TrainingOutputs::TriggerConfig::ONDEMAND);

  writer.AddUmaFeatures(kOutput.data(), kOutput.size(), /*is_output=*/true);
  return std::make_unique<ModelConfig>(std::move(metadata),
                                       kTabResumptionRankerVersion);
}

void TabResumptionRanker::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Tab source inputs and 2 SQL features.
  if (inputs.size() != processing::TabSessionSource::kNumInputs + 2) {
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
