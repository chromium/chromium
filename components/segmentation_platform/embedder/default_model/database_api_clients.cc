// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/segmentation_platform/embedder/default_model/database_api_clients.h"

#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "base/logging.h"
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

// TODO(ssid): Document the purpose and meaning of empty metric names.
constexpr std::array<CustomEvent, 3> kRegisteredCustomEvents{{
    CustomEvent{.event_name = "TestEvents",
                .metric_names = kTestMetricNames.data(),
                .metric_names_size = kTestMetricNames.size()},
    {CustomEvent{.event_name =
                     "NewTabPage.HistoryClusters.FrequentlySeenCategories"}},
    {CustomEvent{.event_name =
                     "NewTabPage.HistoryClusters.FrequentlyEngagedCategories"}},
}};
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
void DatabaseApiClients::AddSumGroupQuery(
    MetadataWriter& writer,
    std::string_view event_name,
    const std::set<std::string>& metric_names,
    int days) {
  DCHECK(!metric_names.empty());
  std::string event_names_sql_criteria;
  for (const auto& metric_name : metric_names) {
    event_names_sql_criteria.append(base::StringPrintf(
        "('%" PRIX64 "'),", base::HashMetricName(metric_name)));
  }
  // Remove the last comma character.
  event_names_sql_criteria.pop_back();

  std::string query = base::StringPrintf(
      "WITH hashed_metric_names(metric_hash) AS (VALUES%s) "
      "SELECT CASE WHEN grouped_metrics.value is null then 0 "
      "else grouped_metrics.value end as metric_value "
      "FROM hashed_metric_names LEFT JOIN "
      "(SELECT metric_hash, SUM(metric_value) as value FROM metrics "
      "WHERE event_hash = '%" PRIX64
      "' "
      "AND event_timestamp BETWEEN ? AND ? "
      "GROUP BY metric_hash) as grouped_metrics "
      "ON hashed_metric_names.metric_hash = grouped_metrics.metric_hash",
      event_names_sql_criteria.c_str(), base::HashMetricName(event_name));

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

// static
void DatabaseApiClients::AddSumQuery(MetadataWriter& writer,
                                     std::string_view metric_name,
                                     int days) {
  std::string query = base::StringPrintf(
      "SELECT SUM(metric_value) FROM metrics WHERE metric_hash = '%" PRIX64
      "' AND event_timestamp BETWEEN ? AND ?",
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
