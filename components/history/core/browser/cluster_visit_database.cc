// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/cluster_visit_database.h"

#include "base/logging.h"
#include "sql/statement.h"
#include "sql/statement_id.h"

namespace history {

namespace {

// An enum of bitmasks to help represent the boolean flags of
// `ClusterVisitContextSignals` in the database. This avoids having to update
// the schema every time we add/remove/change a bool context signal. As these
// are persisted to the database, entries should not be renumbered and numeric
// values should never be reused.
enum class ContextSignalFlags : uint64_t {
  // True if the user has cut or copied the omnibox URL to the clipboard for
  // this page load.
  kOmniboxUrlCopied = 1 << 0,

  // True if the page was in a tab group when the navigation was committed.
  kIsExistingPartOfTabGroup = 1 << 1,

  // True if the page was NOT part of a tab group when the navigation
  // committed, and IS part of a tab group at the end of the page lifetime.
  kIsPlacedInTabGroup = 1 << 2,

  // True if this page was a bookmark when the navigation was committed.
  kIsExistingBookmark = 1 << 3,

  // True if the page was NOT a bookmark when the navigation was committed and
  // was MADE a bookmark during the page's lifetime. In other words:
  // If `is_existing_bookmark` is true, that implies `is_new_bookmark` is false.
  kIsNewBookmark = 1 << 4,

  // True if the page has been explicitly added (by the user) to the list of
  // custom links displayed in the NTP. Links added to the NTP by History
  // TopSites don't count for this.  Always false on Android, because Android
  // does not have NTP custom links.
  kIsNtpCustomLink = 1 << 5,
};

int64_t ContextSignalsToFlags(ClusterVisitContextSignals context_signals) {
  return (context_signals.omnibox_url_copied &
          static_cast<uint64_t>(ContextSignalFlags::kOmniboxUrlCopied)) |
         (context_signals.is_existing_part_of_tab_group &
          static_cast<uint64_t>(
              ContextSignalFlags::kIsExistingPartOfTabGroup)) |
         (context_signals.is_placed_in_tab_group &
          static_cast<uint64_t>(ContextSignalFlags::kIsPlacedInTabGroup)) |
         (context_signals.is_existing_bookmark &
          static_cast<uint64_t>(ContextSignalFlags::kIsExistingBookmark)) |
         (context_signals.is_new_bookmark &
          static_cast<uint64_t>(ContextSignalFlags::kIsNewBookmark)) |
         (context_signals.is_ntp_custom_link &
          static_cast<uint64_t>(ContextSignalFlags::kIsNtpCustomLink));
}

ClusterVisitContextSignals ConstructContextSignalsWithFlags(
    int64_t flags,
    base::TimeDelta duration_since_last_visit,
    int page_end_reason) {
  ClusterVisitContextSignals context_signals;
  context_signals.omnibox_url_copied =
      flags & static_cast<uint64_t>(ContextSignalFlags::kOmniboxUrlCopied);
  context_signals.is_existing_part_of_tab_group =
      flags &
      static_cast<uint64_t>(ContextSignalFlags::kIsExistingPartOfTabGroup);
  context_signals.is_placed_in_tab_group =
      flags & static_cast<uint64_t>(ContextSignalFlags::kIsPlacedInTabGroup);
  context_signals.is_existing_bookmark =
      flags & static_cast<uint64_t>(ContextSignalFlags::kIsExistingBookmark);
  context_signals.is_new_bookmark =
      flags & static_cast<uint64_t>(ContextSignalFlags::kIsNewBookmark);
  context_signals.is_ntp_custom_link =
      flags & static_cast<uint64_t>(ContextSignalFlags::kIsNtpCustomLink);
  context_signals.duration_since_last_visit = duration_since_last_visit;
  context_signals.page_end_reason = page_end_reason;
  return context_signals;
}

// Convenience to construct a `ClusterVisitRow`. Assumes the visit values are
// bound starting at index 0.
ClusterVisitRow StatementToVisitRow(const sql::Statement& statement) {
  return {statement.ColumnInt64(0), statement.ColumnInt64(1),
          statement.ColumnInt64(2),
          ConstructContextSignalsWithFlags(
              statement.ColumnInt64(3),
              base::TimeDelta::FromMicroseconds(statement.ColumnInt64(4)),
              statement.ColumnInt(5))};
}

// Like `StatementToVisitRow()` but for multiple rows.
std::vector<ClusterVisitRow> StatementToVisitRowVector(
    sql::Statement& statement) {
  if (!statement.is_valid())
    return {};
  std::vector<ClusterVisitRow> rows;
  while (statement.Step())
    rows.push_back(StatementToVisitRow(statement));
  return rows;
}

}  // namespace

// Columns, in order, excluding `cluster_visit_id`.
#define HISTORY_CLUSTER_VISIT_ROW_FIELDS_WITHOUT_ID           \
  " url_id, visit_id, cluster_visit_context_signal_bitmask, " \
  "duration_since_last_visit, page_end_reason "

// Columns, in order, including `cluster_visit_id`.
#define HISTORY_CLUSTER_VISIT_ROW_FIELDS \
  " cluster_visit_id," HISTORY_CLUSTER_VISIT_ROW_FIELDS_WITHOUT_ID

ClusterVisitDatabase::ClusterVisitDatabase() = default;

ClusterVisitDatabase::~ClusterVisitDatabase() = default;

bool ClusterVisitDatabase::InitClusterVisitTable() {
  if (!GetDB().DoesTableExist("cluster_visit")) {
    // See `ClusterVisitRow` and `ClusterVisitContextSignals` for details about
    // these fields.
    if (!GetDB().Execute(
            "CREATE TABLE cluster_visit("
            "cluster_visit_id INTEGER PRIMARY KEY,"
            "url_id INTEGER NOT NULL,"
            "visit_id INTEGER NOT NULL,"
            "cluster_visit_context_signal_bitmask INTEGER NOT NULL,"
            "duration_since_last_visit INTEGER NOT NULL,"
            "page_end_reason INTEGER NOT NULL)")) {
      return false;
    }
  }

  return true;
}

bool ClusterVisitDatabase::DropClusterVisitTable() {
  return GetDB().Execute("DROP TABLE cluster_visit");
}

void ClusterVisitDatabase::AddClusterVisit(const ClusterVisitRow& row) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO cluster_visit (" HISTORY_CLUSTER_VISIT_ROW_FIELDS_WITHOUT_ID
      ") VALUES (?, ?, ?, ?, ?)"));
  statement.BindInt64(0, row.url_id);
  statement.BindInt64(1, row.visit_id);
  statement.BindInt64(2, ContextSignalsToFlags(row.context_signals));
  statement.BindInt64(
      3, row.context_signals.duration_since_last_visit.InMicroseconds());
  statement.BindInt(4, row.context_signals.page_end_reason);

  if (!statement.Run()) {
    DVLOG(0) << "Failed to execute cluster visit insert statement:  "
             << "url_id = " << row.url_id << ", visit_id = " << row.visit_id;
  }
}

void ClusterVisitDatabase::DeleteClusterVisit(int64_t cluster_visit_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM cluster_visit WHERE cluster_visit_id = ?"));
  statement.BindInt64(0, cluster_visit_id);

  if (!statement.Run()) {
    DVLOG(0) << "Failed to execute cluster visit delete statement:  "
             << "cluster_visit_id = " << cluster_visit_id;
  }
}

std::vector<ClusterVisitRow> ClusterVisitDatabase::GetClusterVisits(
    int max_results) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" HISTORY_CLUSTER_VISIT_ROW_FIELDS
                     "FROM cluster_visit "
                     "JOIN visits ON visit_id = visits.id "
                     "ORDER BY visits.visit_time DESC "
                     "LIMIT ?"));
  statement.BindInt64(0, max_results);
  return StatementToVisitRowVector(statement);
}

}  // namespace history
