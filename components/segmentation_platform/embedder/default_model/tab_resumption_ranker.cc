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
constexpr uint64_t kTabResumptionRankerVersion = 3;

#define DECL_SQL_FEATURE_SINGLE_UKM_METRIC(name, query, event_name,        \
                                           metric_name)                    \
  constexpr std::array<UkmMetricHash, 1> k##name##_##metric_name##Metrics{ \
      UkmMetricHash::FromUnsafeValue(                                      \
          ukm::builders::event_name::k##metric_name##NameHash),            \
  };                                                                       \
  constexpr std::array<MetadataWriter::SqlFeature::EventAndMetrics, 1>     \
      k##name##_k##metric_name##SqlEvent{                                  \
          MetadataWriter::SqlFeature::EventAndMetrics{                     \
              .event_hash = UkmEventHash::FromUnsafeValue(                 \
                  ukm::builders::event_name::kEntryNameHash),              \
              .metrics = k##name##_##metric_name##Metrics.data(),          \
              .metrics_size = k##name##_##metric_name##Metrics.size()},    \
      };                                                                   \
  constexpr MetadataWriter::SqlFeature name {                              \
    .sql = query, .events = k##name##_k##metric_name##SqlEvent.data(),     \
    .events_size = k##name##_k##metric_name##SqlEvent.size()               \
  }

DECL_SQL_FEATURE_SINGLE_UKM_METRIC(
    kLoadCount,
    "SELECT COUNT(metrics.id) "
    "FROM metrics JOIN urls ON metrics.url_id=urls.url_id "
    "WHERE instr(urls.url,?)>0 AND metrics.metric_hash='64BD7CCE5A95BF00';",
    PageLoad,
    PaintTiming_NavigationToFirstContentfulPaint);

DECL_SQL_FEATURE_SINGLE_UKM_METRIC(
    kForegroundCount,
    "SELECT COUNT(metrics.metric_value) "
    "FROM metrics JOIN urls ON metrics.url_id=urls.url_id "
    "WHERE instr(urls.url,?)>0 AND metrics.metric_hash='D7DB428ED4C5956A';",
    PageLoad,
    PageTiming_TotalForegroundDuration);

DECL_SQL_FEATURE_SINGLE_UKM_METRIC(
    kUsageDuration,
    "SELECT SUM(metrics.metric_value) "
    "FROM metrics JOIN urls ON metrics.url_id=urls.url_id "
    "WHERE instr(urls.url,?)>0 AND metrics.metric_hash='D7DB428ED4C5956A';",
    PageLoad,
    PageTiming_TotalForegroundDuration);

DECL_SQL_FEATURE_SINGLE_UKM_METRIC(
    kIsBookmarked,
    "SELECT SUM(metrics.metric_value) "
    "FROM metrics JOIN urls ON metrics.url_id=urls.url_id "
    "WHERE instr(urls.url,?)>0 AND metrics.metric_hash='356B53FF0A1F60AF';",
    PageLoad,
    IsExistingBookmark);

DECL_SQL_FEATURE_SINGLE_UKM_METRIC(
    kIsPartOfTabGroup,
    "SELECT SUM(metrics.metric_value) "
    "FROM metrics JOIN urls ON metrics.url_id=urls.url_id "
    "WHERE instr(urls.url,?)>0 AND metrics.metric_hash='599EC2B0BC1914C1';",
    PageLoad,
    IsExistingPartOfTabGroup);

DECL_SQL_FEATURE_SINGLE_UKM_METRIC(
    kUrlCopied,
    "SELECT SUM(metrics.metric_value) "
    "FROM metrics JOIN urls ON metrics.url_id=urls.url_id "
    "WHERE instr(urls.url,?)>0 AND metrics.metric_hash='8E3F83DF276E39D1';",
    PageLoad,
    OmniboxUrlCopied);

DECL_SQL_FEATURE_SINGLE_UKM_METRIC(
    kOmniboxShared,
    "SELECT COUNT(metrics.metric_value) "
    "FROM metrics JOIN urls ON metrics.url_id=urls.url_id "
    "WHERE instr(urls.url,?)>0 AND metrics.event_hash='395CEF9417255448';",
    Omnibox_EditUrlSuggestion_Share,
    HasOccurred);

DECL_SQL_FEATURE_SINGLE_UKM_METRIC(
    kMenuShare,
    "SELECT COUNT(metrics.metric_value) "
    "FROM metrics JOIN urls ON metrics.url_id=urls.url_id "
    "WHERE instr(urls.url,?)>0 AND metrics.event_hash='E6D36C300DE14E7A';",
    MobileMenu_Share,
    HasOccurred);

DECL_SQL_FEATURE_SINGLE_UKM_METRIC(
    kTouchScroll,
    "SELECT COUNT(metrics.metric_value) "
    "FROM metrics JOIN urls ON metrics.url_id=urls.url_id "
    "WHERE instr(urls.url,?)>0 AND metrics.metric_hash='A523BF8A1E4EC1C3';",
    Graphics_Smoothness_Latency,
    TouchScroll);

DECL_SQL_FEATURE_SINGLE_UKM_METRIC(
    kTextFieldEdit,
    "SELECT COUNT(metrics.metric_value) "
    "FROM metrics JOIN urls ON metrics.url_id=urls.url_id "
    "WHERE instr(urls.url,?)>0 AND metrics.event_hash='743E2FC8D16C7103';",
    Autofill_TextFieldDidChange,
    FieldTypeGroup);

DECL_SQL_FEATURE_SINGLE_UKM_METRIC(
    kFormEvent,
    "SELECT COUNT(metrics.metric_value) "
    "FROM metrics JOIN urls ON metrics.url_id=urls.url_id "
    "WHERE instr(urls.url,?)>0 AND metrics.event_hash='E6CF82D1CE5CB735';",
    Autofill_FormEvent,
    AutofillFormEvent);

constexpr std::array<MetadataWriter::SqlFeature, 11> kSqlFeatures = {
    kLoadCount,        kForegroundCount, kUsageDuration, kIsBookmarked,
    kIsPartOfTabGroup, kUrlCopied,       kOmniboxShared, kMenuShare,
    kTouchScroll,      kTextFieldEdit,   kFormEvent};

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
      /*min_signal_collection_length_days=*/7,
      /*signal_storage_length_days=*/28);

  // Set features.
  writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = processing::TabSessionSource::kNumInputs,
      .fill_policy = proto::CustomInput::FILL_TAB_METRICS,
      .name = "tab"});
  for (const MetadataWriter::SqlFeature& sql_feature : kSqlFeatures) {
    writer.AddSqlFeature(
        sql_feature,
        {std::make_pair(proto::SqlFeature_BindValue::STRING, kBindValue)});
  }

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
  // Invalid inputs.
  if (inputs.size() !=
      processing::TabSessionSource::kNumInputs + kSqlFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
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
