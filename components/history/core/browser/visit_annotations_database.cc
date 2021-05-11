// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visit_annotations_database.h"

#include <string>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/history/core/browser/url_row.h"
#include "sql/statement.h"
#include "sql/statement_id.h"

namespace history {

namespace {

#define HISTORY_CONTENT_ANNOTATIONS_ROW_FIELDS                               \
  " visit_id, floc_protected_score, categories, page_topics_model_version, " \
  "annotation_flags "
#define HISTORY_CONTEXT_ANNOTATIONS_ROW_FIELDS                       \
  " visit_id, context_annotation_flags, duration_since_last_visit, " \
  "page_end_reason "

// Converts the serialized categories into a vector of (`id`, `weight`)
// pairs.
std::vector<VisitContentModelAnnotations::Category>
GetCategoriesFromStringColumn(const std::string& column_value) {
  std::vector<VisitContentModelAnnotations::Category> categories;

  std::vector<std::string> category_strings = base::SplitString(
      column_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& category_string : category_strings) {
    std::vector<std::string> category_parts = base::SplitString(
        category_string, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (category_parts.size() != 2)
      return {};

    VisitContentModelAnnotations::Category category;
    if (!base::StringToInt(category_parts[0], &category.id))
      continue;
    if (!base::StringToInt(category_parts[1], &category.weight))
      continue;
    categories.emplace_back(category);
  }
  return categories;
}

// Converts categories to something that can be stored in the database.
std::string ConvertCategoriesToStringColumn(
    const std::vector<VisitContentModelAnnotations::Category>& categories) {
  std::vector<std::string> serialized_categories;
  for (const auto& category : categories) {
    serialized_categories.emplace_back(
        base::StrCat({base::NumberToString(category.id), ":",
                      base::NumberToString(category.weight)}));
  }
  return base::JoinString(serialized_categories, ",");
}

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
AnnotatedVisitRow StatementToAnnotatedVisitRow(
    const sql::Statement& statement) {
  return {statement.ColumnInt64(0),
          ConstructContextAnnotationsWithFlags(
              statement.ColumnInt64(1),
              base::TimeDelta::FromMicroseconds(statement.ColumnInt64(2)),
              statement.ColumnInt(3)),
          {}};
}

// Like `StatementToAnnotatedVisitRow()` but for multiple rows.
std::vector<AnnotatedVisitRow> StatementToAnnotatedVisitRowVector(
    sql::Statement& statement) {
  if (!statement.is_valid())
    return {};
  std::vector<AnnotatedVisitRow> rows;
  while (statement.Step())
    rows.push_back(StatementToAnnotatedVisitRow(statement));
  return rows;
}

}  // namespace

VisitAnnotationsDatabase::VisitAnnotationsDatabase() = default;
VisitAnnotationsDatabase::~VisitAnnotationsDatabase() = default;

bool VisitAnnotationsDatabase::InitVisitAnnotationsTables() {
  // Content Annotations table.
  if (!GetDB().DoesTableExist("content_annotations")) {
    if (!GetDB().Execute("CREATE TABLE content_annotations ("
                         "visit_id INTEGER PRIMARY KEY,"
                         "floc_protected_score DECIMAL(3, 2),"
                         "categories VARCHAR,"
                         "page_topics_model_version INTEGER,"
                         "annotation_flags INTEGER DEFAULT 0 NOT NULL)")) {
      return false;
    }
  }

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

bool VisitAnnotationsDatabase::DropVisitAnnotationsTables() {
  // Dropping the tables will implicitly delete the indices.
  return GetDB().Execute("DROP TABLE content_annotations") &&
         GetDB().Execute("DROP TABLE context_annotations");
}

void VisitAnnotationsDatabase::AddContentAnnotationsForVisit(
    VisitID visit_id,
    const VisitContentAnnotations& visit_content_annotations) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO content_annotations (" HISTORY_CONTENT_ANNOTATIONS_ROW_FIELDS
      ") VALUES (?, ?, ?, ?, ?)"));
  statement.BindInt64(0, visit_id);
  statement.BindDouble(
      1, static_cast<double>(
             visit_content_annotations.model_annotations.floc_protected_score));
  statement.BindString(
      2, ConvertCategoriesToStringColumn(
             visit_content_annotations.model_annotations.categories));
  statement.BindInt64(
      3, visit_content_annotations.model_annotations.page_topics_model_version);
  statement.BindInt64(4, visit_content_annotations.annotation_flags);

  if (!statement.Run()) {
    DVLOG(0) << "Failed to execute 'content_annotations' insert statement:  "
             << "visit_id = " << visit_id;
  }
}

void VisitAnnotationsDatabase::AddContextAnnotationsForVisit(
    VisitID visit_id,
    const VisitContextAnnotations& visit_context_annotations) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO context_annotations (" HISTORY_CONTEXT_ANNOTATIONS_ROW_FIELDS
      ") VALUES (?, ?, ?, ?)"));
  statement.BindInt64(0, visit_id);
  statement.BindInt64(1, ContextAnnotationsToFlags(visit_context_annotations));
  statement.BindInt64(
      2, visit_context_annotations.duration_since_last_visit.InMicroseconds());
  statement.BindInt(3, visit_context_annotations.page_end_reason);

  if (!statement.Run()) {
    DVLOG(0)
        << "Failed to execute visit 'context_annotations' insert statement:  "
        << "visit_id = " << visit_id;
  }
}

void VisitAnnotationsDatabase::UpdateContentAnnotationsForVisit(
    VisitID visit_id,
    const VisitContentAnnotations& visit_content_annotations) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "UPDATE content_annotations SET "
                                 "floc_protected_score=?,categories=?,"
                                 "page_topics_model_version=?,"
                                 "annotation_flags=? "
                                 "WHERE visit_id=?"));
  statement.BindDouble(
      0, static_cast<double>(
             visit_content_annotations.model_annotations.floc_protected_score));
  statement.BindString(
      1, ConvertCategoriesToStringColumn(
             visit_content_annotations.model_annotations.categories));
  statement.BindInt64(
      2, visit_content_annotations.model_annotations.page_topics_model_version);
  statement.BindInt64(3, visit_content_annotations.annotation_flags);
  statement.BindInt64(4, visit_id);

  if (!statement.Run()) {
    DVLOG(0)
        << "Failed to execute visit 'content_annotations' update statement:  "
        << "visit_id = " << visit_id;
  }
}

bool VisitAnnotationsDatabase::GetContentAnnotationsForVisit(
    VisitID visit_id,
    VisitContentAnnotations* out_content_annotations) {
  DCHECK(out_content_annotations);

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" HISTORY_CONTENT_ANNOTATIONS_ROW_FIELDS
                     "FROM content_annotations WHERE visit_id=?"));
  statement.BindInt64(0, visit_id);

  if (!statement.Step())
    return false;

  VisitID received_visit_id = statement.ColumnInt64(0);
  // We got a different visit than we asked for, something is wrong.
  DCHECK_EQ(visit_id, received_visit_id);
  if (visit_id != received_visit_id)
    return false;

  out_content_annotations->model_annotations.floc_protected_score =
      static_cast<float>(statement.ColumnDouble(1));
  out_content_annotations->model_annotations.categories =
      GetCategoriesFromStringColumn(statement.ColumnString(2));
  out_content_annotations->model_annotations.page_topics_model_version =
      statement.ColumnInt64(3);
  out_content_annotations->annotation_flags = statement.ColumnInt64(4);
  return true;
}

std::vector<AnnotatedVisitRow> VisitAnnotationsDatabase::GetAnnotatedVisits(
    int max_results) {
  // TODO(manukh): Currently, this only sets the `context_annotations`. It
  //  should also set the `content_annotations`.
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" HISTORY_CONTEXT_ANNOTATIONS_ROW_FIELDS
                     "FROM context_annotations "
                     "JOIN visits ON visit_id = visits.id "
                     "ORDER BY visits.visit_time DESC "
                     "LIMIT ?"));
  statement.BindInt64(0, max_results);
  return StatementToAnnotatedVisitRowVector(statement);
}

std::vector<AnnotatedVisitRow>
VisitAnnotationsDatabase::GetAllContextAnnotationsForTesting() {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" HISTORY_CONTEXT_ANNOTATIONS_ROW_FIELDS
                     "FROM context_annotations"));
  return StatementToAnnotatedVisitRowVector(statement);
}

void VisitAnnotationsDatabase::DeleteAnnotationsForVisit(VisitID visit_id) {
  sql::Statement delete_content_statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM content_annotations WHERE visit_id = ?"));
  delete_content_statement.BindInt64(0, visit_id);
  if (!delete_content_statement.Run()) {
    DVLOG(0) << "Failed to execute 'content_annotations' delete statement:  "
             << "visit_id = " << visit_id;
  }

  sql::Statement delete_context_statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM context_annotations WHERE visit_id = ?"));
  delete_context_statement.BindInt64(0, visit_id);
  if (!delete_context_statement.Run()) {
    DVLOG(0) << "Failed to execute 'context_annotations' delete statement:  "
             << "visit_id = " << visit_id;
  }
}

bool VisitAnnotationsDatabase::MigrateFlocAllowedToAnnotationsTable() {
  if (!GetDB().DoesTableExist("content_annotations")) {
    NOTREACHED() << " content_annotations table should exist before migration";
    return false;
  }

  // Not all version 43 history has the content_annotations table. So at this
  // point the content_annotations table may already have been initialized with
  // the latest version with a annotation_flags column.
  if (!GetDB().DoesColumnExist("content_annotations", "annotation_flags")) {
    // Add a annotation_flags column to the content_annotations table.
    if (!GetDB().Execute("ALTER TABLE content_annotations ADD COLUMN "
                         "annotation_flags INTEGER DEFAULT 0 NOT NULL")) {
      return false;
    }
  }

  // If there's a matching visit entry in the content_annotations table, migrate
  // the publicly_routable field from the visit entry to the
  // annotation_flags field of the annotation entry.
  if (!GetDB().Execute("UPDATE content_annotations "
                       "SET annotation_flags=1 "
                       "FROM visits "
                       "WHERE visits.id=content_annotations.visit_id AND "
                       "visits.publicly_routable")) {
    return false;
  }

  // Migrate all publicly_routable visit entries that don't have a matching
  // entry in the content_annotations table. The rest of the fields are set to
  // their default value.
  if (!GetDB().Execute("INSERT OR IGNORE INTO content_annotations "
                       "(visit_id,floc_protected_score,categories,"
                       "page_topics_model_version,annotation_flags) "
                       "SELECT id,-1,'',-1,1 FROM visits "
                       "WHERE visits.publicly_routable")) {
    return false;
  }

  return true;
}

bool VisitAnnotationsDatabase::MigrateReplaceClusterVisitsTable() {
  // We don't need to actually copy values from the previous table; it was only
  // rolled out behind a flag.
  return !GetDB().DoesTableExist("cluster_visits") ||
         GetDB().Execute("DROP TABLE cluster_visits");
}

}  // namespace history
