// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <cstdint>

#include "components/segmentation_platform/internal/database/uma_metrics_table.h"

#include "base/strings/stringprintf.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "sql/statement.h"

namespace segmentation_platform {
namespace {

std::string UmaHashToDBString(uint64_t metric_hash) {
  return base::StringPrintf("%" PRIX64, metric_hash);
}

}  // namespace

UmaMetricsTable::UmaMetricsTable(sql::Database* db) : db_(db) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

UmaMetricsTable::~UmaMetricsTable() = default;

bool UmaMetricsTable::InitTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kCreateTableQuery[] =
      // clang-format off
      "CREATE TABLE IF NOT EXISTS uma_metrics("
          "id INTEGER PRIMARY KEY NOT NULL,"
          "event_timestamp INTEGER NOT NULL,"
          "profile_id INTEGER NOT NULL,"
          "type INTEGER NOT NULL,"
          "metric_hash TEXT NOT NULL,"
          "metric_value INTEGER NOT NULL)";
  // clang-format on
  if (!db_->Execute(kCreateTableQuery)) {
    return false;
  }

  // Accelerates get entries for a range of event_timestamps.
  if (!db_->Execute("CREATE INDEX IF NOT EXISTS uma_event_timestamp_index ON "
                    "uma_metrics(event_timestamp)")) {
    return false;
  }
  // Accelerates update entries for a given profile_id.
  if (!db_->Execute("CREATE INDEX IF NOT EXISTS uma_profile_id_index ON "
                    "uma_metrics(profile_id)")) {
    return false;
  }
  // Accelerates find metrics for a given type.
  if (!db_->Execute("CREATE INDEX IF NOT EXISTS uma_type_index ON "
                    "uma_metrics(type)")) {
    return false;
  }
  // Accelerates find value for a given metric_hash.
  if (!db_->Execute("CREATE INDEX IF NOT EXISTS uma_metric_hash_index ON "
                    "uma_metrics(metric_hash)")) {
    return false;
  }

  return true;
}

bool UmaMetricsTable::AddUmaMetric(const std::string& profile_id,
                                   const UmaMetricEntry& row) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (row.type == proto::SignalType::UNKNOWN_SIGNAL_TYPE ||
      profile_id.empty() || row.time.is_null()) {
    return false;
  }
  static constexpr char kInsertQuery[] =
      // clang-format off
      "INSERT INTO uma_metrics(event_timestamp,profile_id,type,metric_hash,"
          "metric_value)"
          "VALUES (?,?,?,?,?)";
  // clang-format on

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertQuery));
  statement.BindTime(0, row.time);
  statement.BindString(1, profile_id);
  statement.BindInt64(2, row.type);
  statement.BindString(3, UmaHashToDBString(row.name_hash));
  statement.BindInt64(4, row.value);
  return statement.Run();
}

bool UmaMetricsTable::DeleteEventsBeforeTimestamp(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kDeleteoldEntries[] =
      "DELETE FROM uma_metrics WHERE event_timestamp<=?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteoldEntries));
  statement.BindTime(0, time);
  return statement.Run();
}

bool UmaMetricsTable::CleanupItems(
    const std::string& profile_id,
    const std::vector<CleanupItem>& cleanup_items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kDeleteOldEntry[] =
      "DELETE FROM uma_metrics "
      "WHERE profile_id=? AND metric_hash=? AND type=? AND event_timestamp<=?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteOldEntry));
  for (const auto& item : cleanup_items) {
    statement.Reset(/*clear_bound_vars=*/false);
    statement.BindString(0, profile_id);
    statement.BindString(1, UmaHashToDBString(item.name_hash));
    statement.BindInt64(2, item.signal_type);
    statement.BindTime(3, item.timestamp);
    if (!statement.Run()) {
      return false;
    }
  }
  return true;
}

}  // namespace segmentation_platform
