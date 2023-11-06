// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/database_api_clients.h"

#include <memory>

#include "base/metrics/metrics_hashes.h"
#include "base/strings/stringprintf.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;
constexpr SegmentId kSegmentId = SegmentId::DATABASE_API_CLIENTS;
constexpr int64_t kModelVersion = 1;

// List of custom events used by DatabaseClient API users.
// TODO(ssid): Add TTL for the custom events to be deleted. Currently they will
// get deleted after `kUkmEntriesTTL`.
struct CustomEvent {
  // The event or the project name.
  const char* const event_name;
  // List of metric names:
  const raw_ptr<const char* const> metric_names;
  const size_t metric_names_size;
};

// ----------------------------------------------------------------------------
// List of metrics stored in database by DatabaseClient API users.
// TODO(ssid): UMA and UKM metrics can be listed here, add examples.
constexpr std::array<const char*, 3> kTestMetricNames{"test1", "test2",
                                                      "test3"};

constexpr std::array<CustomEvent, 1> kRegisteredCustomEvents{
    {CustomEvent{.event_name = "TestEvents",
                 .metric_names = kTestMetricNames.data(),
                 .metric_names_size = kTestMetricNames.size()}},
};
// End of metrics list.
// ----------------------------------------------------------------------------

void AddCustomEvent(const CustomEvent& custom_event, MetadataWriter& writer) {
  std::vector<UkmMetricHash> metrics;
  for (unsigned j = 0; j < custom_event.metric_names_size; ++j) {
    metrics.emplace_back(
        base::HashMetricName(custom_event.metric_names.get()[j]));
  }
  MetadataWriter::SqlFeature::EventAndMetrics event{
      .event_hash = UkmEventHash(base::HashMetricName(custom_event.event_name)),
      .metrics = metrics.data(),
      .metrics_size = metrics.size(),
  };
  MetadataWriter::SqlFeature sql_feature{
      .sql = "select 1;",
      .events = &event,
      .events_size = 1,
  };
  writer.AddSqlFeature(sql_feature, {});
}

}  // namespace

// static
std::unique_ptr<Config> DatabaseApiClients::GetConfig() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kDatabaseApiClientsKey;
  config->segmentation_uma_name = kDatabaseApiClientsUmaName;
  config->AddSegmentId(kSegmentId, std::make_unique<DatabaseApiClients>());
  config->auto_execute_and_cache = false;
  return config;
}

// static
void DatabaseApiClients::AddSumQuery(MetadataWriter& writer,
                                     base::StringPiece metric_name,
                                     int days) {
  std::string query = base::StringPrintf(
      "SELECT SUM(metric_value) FROM metrics WHERE metric_hash = '%" PRIX64
      "' AND  event_timestamp BETWEEN ? AND ?",
      base::HashMetricName(metric_name));
  MetadataWriter::SqlFeature sql_feature{.sql = query.c_str()};
  std::string days_str = base::StringPrintf("%d", days);
  const std::array<MetadataWriter::CustomInput::Arg, 1> kBindValueArg{
      std::make_pair("bucket_count", days_str.c_str())};
  const MetadataWriter::BindValue kBindValue{
      proto::SqlFeature::BindValue::TIME,
      {.tensor_length = 2,
       .fill_policy = proto::CustomInput::TIME_RANGE_BEFORE_PREDICTION,
       .arg = kBindValueArg.data(),
       .arg_size = kBindValueArg.size()}};
  writer.AddSqlFeature(sql_feature, {kBindValue});
}

DatabaseApiClients::DatabaseApiClients() : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
DatabaseApiClients::GetModelConfig() {
  // Write a valid dummy metadata, mainly used to track metric hashes.
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(0);
  writer.AddOutputConfigForBinaryClassifier(0.5, "N/A", kLegacyNegativeLabel);

  for (const CustomEvent& custom_event : kRegisteredCustomEvents) {
    AddCustomEvent(custom_event, writer);
  }
  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void DatabaseApiClients::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // This model should not be executed, only used for tracking custom metrics.
  CHECK(0);
}

}  // namespace segmentation_platform
