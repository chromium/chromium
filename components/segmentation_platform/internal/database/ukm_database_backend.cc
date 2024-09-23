// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_database_backend.h"

#include <vector>

#include "base/check_is_test.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/database/ukm_metrics_table.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/database/ukm_url_table.h"
#include "components/segmentation_platform/internal/database/uma_metrics_table.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace segmentation_platform {

namespace {

BASE_FEATURE(kSqlWALModeOnSegmentationDatabase,
             "SqlWALModeOnSegmentationDatabase",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Up to 10 updates are batched, because ~10 UKM metrics recorded in db per
// page load and approximately a commit every page load. This might need update
// if the metric count increases in the future.
static constexpr int kChangeCountToCommit = 10;

bool SanityCheckUrl(const GURL& url, UrlId url_id) {
  return url.is_valid() && !url.is_empty() && !url_id.is_null();
}

std::string BindValuesToStatement(
    const std::vector<processing::ProcessedValue>& bind_values,
    sql::Statement& statement) {
  std::stringstream debug_string;
  for (unsigned i = 0; i < bind_values.size(); ++i) {
    const processing::ProcessedValue& value = bind_values[i];
    switch (value.type) {
      case processing::ProcessedValue::Type::BOOL:
        debug_string << i << ":" << value.bool_val << " ";
        statement.BindBool(i, value.bool_val);
        break;
      case processing::ProcessedValue::Type::INT:
        debug_string << i << ":" << value.int_val << " ";
        statement.BindInt(i, value.int_val);
        break;
      case processing::ProcessedValue::Type::FLOAT:
        debug_string << i << ":" << value.float_val << " ";
        statement.BindDouble(i, value.float_val);
        break;
      case processing::ProcessedValue::Type::DOUBLE:
        debug_string << i << ":" << value.double_val << " ";
        statement.BindDouble(i, value.double_val);
        break;
      case processing::ProcessedValue::Type::STRING:
        debug_string << i << ":" << value.str_val << " ";
        statement.BindString(i, value.str_val);
        break;
      case processing::ProcessedValue::Type::TIME:
        debug_string << i << ":" << value.time_val << " ";
        statement.BindTime(i, value.time_val);
        break;
      case processing::ProcessedValue::Type::INT64:
        debug_string << i << ":" << value.int64_val << " ";
        statement.BindInt64(i, value.int64_val);
        break;
      case processing::ProcessedValue::Type::URL:
        debug_string << i << ":"
                     << UkmUrlTable::GetDatabaseUrlString(*value.url) << " ";
        statement.BindString(i, UkmUrlTable::GetDatabaseUrlString(*value.url));
        break;
      case processing::ProcessedValue::Type::UNKNOWN:
        NOTREACHED_IN_MIGRATION();
    }
  }
  return debug_string.str();
}

float GetSingleFloatOutput(sql::Statement& statement) {
  sql::ColumnType output_type = statement.GetColumnType(0);
  switch (output_type) {
    case sql::ColumnType::kBlob:
    case sql::ColumnType::kText:
      NOTREACHED_IN_MIGRATION();
      return 0;
    case sql::ColumnType::kFloat:
      return statement.ColumnDouble(0);
    case sql::ColumnType::kInteger:
      return statement.ColumnInt64(0);
    case sql::ColumnType::kNull:
      return 0;
  }
}

void ErrorCallback(int code, sql::Statement* stmt) {
  VLOG(1) << "SQL run error " << code;
}

}  // namespace

UkmDatabaseBackend::UkmDatabaseBackend(
    const base::FilePath& database_path,
    bool in_memory,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
    : database_path_(database_path),
      in_memory_(in_memory),
      callback_task_runner_(callback_task_runner),
      db_(sql::DatabaseOptions{.wal_mode = base::FeatureList::IsEnabled(
                                   kSqlWALModeOnSegmentationDatabase)}),
      metrics_table_(&db_),
      url_table_(&db_),
      uma_metrics_table_(&db_) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  db_.set_histogram_tag("UKMMetrics");
  db_.set_error_callback(base::BindRepeating(&ErrorCallback));
}

UkmDatabaseBackend::~UkmDatabaseBackend() {
  if (current_transaction_) {
    current_transaction_->Commit();
    current_transaction_.reset();
  }
}

void UkmDatabaseBackend::InitDatabase(SuccessCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SCOPED_UMA_HISTOGRAM_TIMER("SegmentationPlatform.UkmDatabase.InitTime");
  base::File::Error error{};
  bool result = true;
  if (in_memory_) {
    CHECK_IS_TEST();
    result = db_.OpenInMemory();
  } else if (!base::CreateDirectoryAndGetError(database_path_.DirName(),
                                               &error) ||
             !db_.Open(database_path_)) {
    // TODO(ssid): On failure retry opening the database or delete backend or
    // open in memory for session.
    LOG(ERROR) << "Failed to open UKM database: " << error << " "
               << db_.GetErrorMessage();
    result = false;
  }
  if (result) {
    result = metrics_table_.InitTable() && url_table_.InitTable() &&
             uma_metrics_table_.InitTable();
  }
  status_ = result ? Status::INIT_SUCCESS : Status::INIT_FAILED;

  if (status_ == Status::INIT_SUCCESS) {
    RestartTransaction();
  }
  callback_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(callback), result));
}

void UkmDatabaseBackend::StoreUkmEntry(ukm::mojom::UkmEntryPtr entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SCOPED_UMA_HISTOGRAM_TIMER("SegmentationPlatform.Database.StoreUkmEntry");
  if (status_ != Status::INIT_SUCCESS) {
    return;
  }

  MetricsRowEventId event_id =
      MetricsRowEventId::FromUnsafeValue(base::RandUint64());
  // If we have an URL ID for the entry, then use it, otherwise the URL ID will
  // be updated when to all metrics when UpdateUrlForUkmSource() is called.
  UrlId url_id;
  auto it = source_to_url_.find(entry->source_id);
  if (it != source_to_url_.end())
    url_id = it->second;

  UkmMetricsTable::MetricsRow row = {
      .event_timestamp = base::Time::Now(),
      .url_id = url_id,
      .source_id = entry->source_id,
      .event_id = event_id,
      .event_hash = UkmEventHash::FromUnsafeValue(entry->event_hash)};
  for (const auto& metric_and_value : entry->metrics) {
    row.metric_hash = UkmMetricHash::FromUnsafeValue(metric_and_value.first);
    row.metric_value = metric_and_value.second;
    metrics_table_.AddUkmEvent(row);
  }
  TrackChangesInTransaction(entry->metrics.size());
}

void UkmDatabaseBackend::UpdateUrlForUkmSource(ukm::SourceId source_id,
                                               const GURL& url,
                                               bool is_validated,
                                               const std::string& profile_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SCOPED_UMA_HISTOGRAM_TIMER(
      "SegmentationPlatform.Database.UpdateUrlForUkmSource");
  if (status_ != Status::INIT_SUCCESS) {
    return;
  }

  UrlId url_id = UkmUrlTable::GenerateUrlId(url);
  if (!SanityCheckUrl(url, url_id)) {
    return;
  }

  if (!url_table_.IsUrlInTable(url_id)) {
    if (is_validated) {
      url_table_.WriteUrl(url, url_id, base::Time::Now(), profile_id);
      // Remove from list so we don't add the URL again to table later.
      urls_not_validated_.erase(url_id);
    } else {
      urls_not_validated_.insert(url_id);
    }
  } else {
    url_table_.UpdateUrlTimestamp(url_id, base::Time::Now());
  }
  // Keep track of source to URL ID mapping for future metrics.
  source_to_url_[source_id] = url_id;
  // Update all entries in metrics table with the URL ID.
  metrics_table_.UpdateUrlIdForSource(source_id, url_id);

  TrackChangesInTransaction(2);  // 2 updates above.
}

void UkmDatabaseBackend::OnUrlValidated(const GURL& url,
                                        const std::string& profile_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status_ != Status::INIT_SUCCESS) {
    return;
  }

  UrlId url_id = UkmUrlTable::GenerateUrlId(url);
  // Write URL to table only if it's needed and it's not already added.
  if (urls_not_validated_.count(url_id) && SanityCheckUrl(url, url_id)) {
    url_table_.WriteUrl(url, url_id, base::Time::Now(), profile_id);
    urls_not_validated_.erase(url_id);
  }
  TrackChangesInTransaction(1);
}

void UkmDatabaseBackend::RemoveUrls(const std::vector<GURL>& urls,
                                    bool all_urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SCOPED_UMA_HISTOGRAM_TIMER("SegmentationPlatform.Database.RemoveUrls");
  if (status_ != Status::INIT_SUCCESS) {
    return;
  }

  if (all_urls) {
    DeleteAllUrls();
    return;
  }

  std::vector<UrlId> url_ids;
  for (const GURL& url : urls) {
    UrlId id = UkmUrlTable::GenerateUrlId(url);
    // Do not accidentally remove all entries without URL (kInvalidUrlID).
    if (!SanityCheckUrl(url, id))
      continue;
    url_ids.push_back(id);
    urls_not_validated_.erase(id);
  }
  url_table_.RemoveUrls(url_ids);
  metrics_table_.DeleteEventsForUrls(url_ids);

  // Force commit so that we don't store URLs longer than needed.
  RestartTransaction();
}

void UkmDatabaseBackend::AddUmaMetric(const std::string& profile_id,
                                      const UmaMetricEntry& row) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SCOPED_UMA_HISTOGRAM_TIMER("SegmentationPlatform.Database.AddUmaMetric");
  if (status_ != Status::INIT_SUCCESS) {
    return;
  }
  uma_metrics_table_.AddUmaMetric(profile_id, row);
}

void UkmDatabaseBackend::RunReadOnlyQueries(QueryList&& queries,
                                            QueryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SCOPED_UMA_HISTOGRAM_TIMER(
      "SegmentationPlatform.Database.RunReadOnlyQueries");
  if (status_ != Status::INIT_SUCCESS) {
    callback_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false,
                                  processing::IndexedTensors()));
    return;
  }

  bool success = true;
  processing::IndexedTensors result;
  for (const auto& index_and_query : queries) {
    const processing::FeatureIndex index = index_and_query.first;
    const UkmDatabase::CustomSqlQuery& query = index_and_query.second;
    std::string debug_query = query.query;

    sql::Statement statement(db_.GetReadonlyStatement(query.query));
    debug_query +=
        " Bind values: " + BindValuesToStatement(query.bind_values, statement);

    if (!statement.is_valid()) {
      VLOG(1) << "Failed to run SQL query " << debug_query;
      success = false;
      break;
    }
    while (statement.Step()) {
      float output = GetSingleFloatOutput(statement);
      result[index].push_back(processing::ProcessedValue::FromFloat(output));
    }
    if (!result.count(index) || result.at(index).empty() ||
        !statement.Succeeded()) {
      VLOG(1) << "Failed to run SQL query " << debug_query;
      success = false;
      break;
    }

    if (VLOG_IS_ON(1)) {
      std::string outputs;
      for (const auto& val : result[index]) {
        outputs.append(base::StringPrintf("%f,", val.float_val));
      }
      VLOG(1) << "Output from SQL query " << debug_query
              << " Result: " << outputs;
    }
  }
  callback_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), success, std::move(result)));
}

void UkmDatabaseBackend::DeleteEntriesOlderThan(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status_ != Status::INIT_SUCCESS) {
    return;
  }

  std::vector<UrlId> deleted_urls =
      metrics_table_.DeleteEventsBeforeTimestamp(time);
  url_table_.RemoveUrls(deleted_urls);
  url_table_.DeleteUrlsBeforeTimestamp(time);

  // Force commit so that we don't store URLs longer than needed.
  RestartTransaction();
}

void UkmDatabaseBackend::CleanupItems(const std::string& profile_id,
                                      std::vector<CleanupItem> cleanup_items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status_ != Status::INIT_SUCCESS) {
    return;
  }

  // This needs to support clean up for UKM data.
  // Only `cleanup_items` with uma types should be sent to uma table.
  std::erase_if(cleanup_items,
                [](const CleanupItem& item) { return !item.IsUma(); });
  uma_metrics_table_.CleanupItems(profile_id, cleanup_items);
  TrackChangesInTransaction(cleanup_items.size());
}

void UkmDatabaseBackend::CommitTransactionForTesting() {
  RestartTransaction();
}

void UkmDatabaseBackend::RollbackTransactionForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(current_transaction_);
  current_transaction_->Rollback();
  current_transaction_.reset();
}

void UkmDatabaseBackend::DeleteAllUrls() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(status_, Status::INIT_SUCCESS);

  // Remove all metrics associated with any URL, but retain the metrics that are
  // not keyed on URL.
  bool success = db_.Execute("DELETE FROM metrics WHERE url_id!=0");
  // TODO(ssid): sqlite uses truncate optimization on DELETE statements without
  // WHERE clause. Maybe replace the DROP and CREATE with DELETE if the
  // performance is better.
  success = success && db_.Execute("DROP TABLE urls");
  success = success && url_table_.InitTable();
  DCHECK(success);
  RestartTransaction();
}

void UkmDatabaseBackend::TrackChangesInTransaction(int change_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No transaction has begun, begin one.
  if (!current_transaction_) {
    RestartTransaction();
    // Ignore change_count since no transaction has begun yet.
    return;
  }

  change_count_ += change_count;

  // If enough changes are made, commit them and begin a new transaction.
  if (change_count_ > kChangeCountToCommit) {
    RestartTransaction();
  }
}

void UkmDatabaseBackend::RestartTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (current_transaction_) {
    current_transaction_->Commit();
    current_transaction_.reset();
  }

  change_count_ = 0;
  current_transaction_ = std::make_unique<sql::Transaction>(&db_);
  if (!current_transaction_->Begin()) {
    current_transaction_.reset();
  }
}

}  // namespace segmentation_platform
