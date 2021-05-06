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
// `VisitContextAnnotations` in the database. This avoids having to update
// the schema every time we add/remove/change a bool context annotation. As
// these are persisted to the database, entries should not be renumbered and
// numeric values should never be reused.
enum class ContextAnnotationFlags : uint64_t {
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

int64_t ContextAnnotationsToFlags(VisitContextAnnotations context_annotations) {
  return (context_annotations.omnibox_url_copied &
          static_cast<uint64_t>(ContextAnnotationFlags::kOmniboxUrlCopied)) |
         (context_annotations.is_existing_part_of_tab_group &
          static_cast<uint64_t>(
              ContextAnnotationFlags::kIsExistingPartOfTabGroup)) |
         (context_annotations.is_placed_in_tab_group &
          static_cast<uint64_t>(ContextAnnotationFlags::kIsPlacedInTabGroup)) |
         (context_annotations.is_existing_bookmark &
          static_cast<uint64_t>(ContextAnnotationFlags::kIsExistingBookmark)) |
         (context_annotations.is_new_bookmark &
          static_cast<uint64_t>(ContextAnnotationFlags::kIsNewBookmark)) |
         (context_annotations.is_ntp_custom_link &
          static_cast<uint64_t>(ContextAnnotationFlags::kIsNtpCustomLink));
}

VisitContextAnnotations ConstructContextAnnotationsWithFlags(
    int64_t flags,
    base::TimeDelta duration_since_last_visit,
    int page_end_reason) {
  VisitContextAnnotations context_annotations;
  context_annotations.omnibox_url_copied =
      flags & static_cast<uint64_t>(ContextAnnotationFlags::kOmniboxUrlCopied);
  context_annotations.is_existing_part_of_tab_group =
      flags &
      static_cast<uint64_t>(ContextAnnotationFlags::kIsExistingPartOfTabGroup);
  context_annotations.is_placed_in_tab_group =
      flags &
      static_cast<uint64_t>(ContextAnnotationFlags::kIsPlacedInTabGroup);
  context_annotations.is_existing_bookmark =
      flags &
      static_cast<uint64_t>(ContextAnnotationFlags::kIsExistingBookmark);
  context_annotations.is_new_bookmark =
      flags & static_cast<uint64_t>(ContextAnnotationFlags::kIsNewBookmark);
  context_annotations.is_ntp_custom_link =
      flags & static_cast<uint64_t>(ContextAnnotationFlags::kIsNtpCustomLink);
  context_annotations.duration_since_last_visit = duration_since_last_visit;
  context_annotations.page_end_reason = page_end_reason;
  return context_annotations;
}

// Convenience to construct a `AnnotatedVisitRow`. Assumes the visit values are
// bound starting at index 0.
AnnotatedVisitRow StatementToVisitRow(const sql::Statement& statement) {
  return {statement.ColumnInt64(0),
          ConstructContextAnnotationsWithFlags(
              statement.ColumnInt64(1),
              base::TimeDelta::FromMicroseconds(statement.ColumnInt64(2)),
              statement.ColumnInt(3)),
          {}};
}

// Like `StatementToVisitRow()` but for multiple rows.
std::vector<AnnotatedVisitRow> StatementToVisitRowVector(
    sql::Statement& statement) {
  if (!statement.is_valid())
    return {};
  std::vector<AnnotatedVisitRow> rows;
  while (statement.Step())
    rows.push_back(StatementToVisitRow(statement));
  return rows;
}

}  // namespace

// Columns, in order.
#define HISTORY_CLUSTER_VISIT_ROW_FIELDS                             \
  " visit_id, context_annotation_flags, duration_since_last_visit, " \
  "page_end_reason "

ClusterVisitDatabase::ClusterVisitDatabase() = default;

ClusterVisitDatabase::~ClusterVisitDatabase() = default;

bool ClusterVisitDatabase::InitClusterVisitTable() {
  if (!GetDB().DoesTableExist("context_annotations")) {
    // See `AnnotatedVisitRow` and `VisitContextAnnotations` for details about
    // these fields.
    if (!GetDB().Execute("CREATE TABLE context_annotations("
                         "visit_id INTEGER PRIMARY KEY,"
                         "context_annotation_flags INTEGER DEFAULT 0 NOT NULL,"
                         "duration_since_last_visit INTEGER,"
                         "page_end_reason INTEGER)")) {
      return false;
    }
  }

  return true;
}

bool ClusterVisitDatabase::DropClusterVisitTable() {
  return GetDB().Execute("DROP TABLE context_annotations");
}

void ClusterVisitDatabase::AddAnnotatedVisit(const AnnotatedVisitRow& row) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO context_annotations (" HISTORY_CLUSTER_VISIT_ROW_FIELDS
      ") VALUES (?, ?, ?, ?)"));
  statement.BindInt64(0, row.visit_id);
  statement.BindInt64(1, ContextAnnotationsToFlags(row.context_annotations));
  statement.BindInt64(
      2, row.context_annotations.duration_since_last_visit.InMicroseconds());
  statement.BindInt(3, row.context_annotations.page_end_reason);

  if (!statement.Run()) {
    DVLOG(0) << "Failed to execute annotated visit insert statement:  "
             << "visit_id = " << row.visit_id;
  }
}

void ClusterVisitDatabase::DeleteAnnotatedVisit(VisitID visit_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM context_annotations WHERE visit_id = ?"));
  statement.BindInt64(0, visit_id);

  if (!statement.Run()) {
    DVLOG(0) << "Failed to execute annotated visit delete statement:  "
             << "visit_id = " << visit_id;
  }
}

std::vector<AnnotatedVisitRow> ClusterVisitDatabase::GetAnnotatedVisits(
    int max_results) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" HISTORY_CLUSTER_VISIT_ROW_FIELDS
                     "FROM context_annotations "
                     "JOIN visits ON visit_id = visits.id "
                     "ORDER BY visits.visit_time DESC "
                     "LIMIT ?"));
  statement.BindInt64(0, max_results);
  return StatementToVisitRowVector(statement);
}

bool ClusterVisitDatabase::MigrateReplaceClusterVisitsTable() {
  // We don't need to actually copy values from the previous table; it's only
  // rolled out behind a flag.
  return !GetDB().DoesTableExist("cluster_visits") ||
         GetDB().Execute("DROP TABLE cluster_visits");
}

}  // namespace history
