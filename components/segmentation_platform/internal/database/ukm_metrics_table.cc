// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_metrics_table.h"

#include <inttypes.h>
#include <cstdint>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace segmentation_platform {

UkmMetricsTable::UkmMetricsTable(sql::Database* db) : db_(db) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(db_);
}

UkmMetricsTable::~UkmMetricsTable() = default;

bool UkmMetricsTable::InitTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kCreateTableQuery[] =
      // clang-format off
      "CREATE TABLE IF NOT EXISTS metrics("
          "id INTEGER PRIMARY KEY NOT NULL,"
          "event_timestamp INTEGER NOT NULL,"
          "ukm_source_id INTEGER NOT NULL,"
          "url_id INTEGER NOT NULL,"
          "event_id INTEGER NOT NULL,"
          "event_hash TEXT NOT NULL,"
          "metric_hash TEXT NOT NULL,"
          "metric_value INTEGER NOT NULL)";
  // clang-format on
  if (!db_->Execute(kCreateTableQuery)) {
    return false;
  }

  // Accelerates get entries for a range of event_timestamps.
  if (!db_->Execute("CREATE INDEX IF NOT EXISTS event_timestamp_index ON "
                    "metrics(event_timestamp)")) {
    return false;
  }
  // Accelerates update entries for a given ukm_source_id.
  if (!db_->Execute("CREATE INDEX IF NOT EXISTS ukm_source_id_index ON "
                    "metrics(ukm_source_id)")) {
    return false;
  }
  // Accelerates find metrics for a given url_id, join with URL table using
  // url_id.
  if (!db_->Execute("CREATE INDEX IF NOT EXISTS url_id_index ON "
                    "metrics(url_id)")) {
    return false;
  }
  // Accelerates find value for a given event_hash and metric_hash.
  if (!db_->Execute("CREATE INDEX IF NOT EXISTS event_hash_index ON "
                    "metrics(event_hash)")) {
    return false;
  }

  // Other common queries using the metrics table:
  // * Join metrics table with itself using event_id, to find metrics of the
  //   same event.
  //
  // Metric hashes should not be used to query on the database unless event hash
  // is also used as filter. So, just having an index for event_hash is good
  // enough. Event ID is also used only after filtering the table by the
  // event_hash. So, event_id does not need an index.

  return true;
}

bool UkmMetricsTable::AddUkmEvent(const UkmMetricsTable::MetricsRow& row) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!row.id);
  // Verify visit row.
  if (row.metric_hash.is_null() || row.event_hash.is_null()) {
    return false;
  }
  if (row.event_timestamp.is_null() ||
      row.event_timestamp < base::Time::Now() - kNumDaysToKeepUkm) {
    return false;
  }

  static constexpr char kInsertQuery[] =
      // clang-format off
      "INSERT INTO metrics(event_timestamp,ukm_source_id,url_id,event_id,"
          "event_hash,metric_hash,metric_value) "
          "VALUES (?,?,?,?,?,?,?)";
  // clang-format on

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertQuery));
  statement.BindTime(0, row.event_timestamp);
  statement.BindInt64(1, row.source_id);
  statement.BindInt64(2, row.url_id.GetUnsafeValue());
  statement.BindInt64(3, row.event_id.GetUnsafeValue());
  uint64_t event_hash = row.event_hash.GetUnsafeValue();
  statement.BindString(4, base::StringPrintf("%" PRIX64, event_hash));
  uint64_t metric_hash = row.metric_hash.GetUnsafeValue();
  statement.BindString(5, base::StringPrintf("%" PRIX64, metric_hash));
  statement.BindInt64(6, row.metric_value);
  return statement.Run();
}

std::string HashToHexString(uint64_t hash) {
  return base::StringPrintf("%" PRIX64, hash);
}

bool UkmMetricsTable::UpdateUrlIdForSource(ukm::SourceId source_id,
                                           UrlId url_id) {
  DCHECK_NE(source_id, ukm::kInvalidSourceId);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kUpdateUrlIdQuery[] =
      "UPDATE metrics SET url_id = ? WHERE ukm_source_id = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kUpdateUrlIdQuery));
  statement.BindInt64(0, url_id.GetUnsafeValue());
  statement.BindInt64(1, source_id);

  return statement.Run();
}

bool UkmMetricsTable::DeleteEventsForUrls(const std::vector<UrlId>& urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kDeleteForUrl[] =
      "DELETE FROM metrics WHERE url_id = ?";
  bool success = true;
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteForUrl));
  for (const UrlId& url_id : urls) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, url_id.GetUnsafeValue());
    if (!statement.Run()) {
      success = false;
    }
  }
  return success;
}

std::vector<UrlId> UkmMetricsTable::DeleteEventsBeforeTimestamp(
    base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<UrlId> url_list;

  // Get a list of URL IDs of the metrics that will be removed.
  static constexpr char kGetOldEntries[] =
      "SELECT DISTINCT url_id FROM metrics WHERE event_timestamp<=? ORDER BY "
      "url_id";
  sql::Statement find_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetOldEntries));
  find_statement.BindTime(0, time);
  while (find_statement.Step()) {
    url_list.push_back(UrlId::FromUnsafeValue(find_statement.ColumnInt64(0)));
  }

  // Delete the metrics.
  static constexpr char kDeleteoldEntries[] =
      "DELETE FROM metrics WHERE event_timestamp<=?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteoldEntries));
  statement.BindTime(0, time);
  if (!statement.Run()) {
    return {};
  }

  // Find the list of URL IDs that are no longer needed in the URL table by
  // checking if there are any other metrics referring to the removed URL IDs.
  for (auto it = url_list.begin(); it != url_list.end();) {
    if (HasEntriesWithUrl(*it)) {
      it = url_list.erase(it);
    } else {
      it++;
    }
  }
  return url_list;
}

bool UkmMetricsTable::HasEntriesWithUrl(UrlId url_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kGetUrlQuery[] =
      "SELECT 1 FROM metrics WHERE url_id=? LIMIT 1";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetUrlQuery));
  statement.BindInt64(0, url_id.GetUnsafeValue());
  return statement.Step();
}

}  // namespace segmentation_platform
