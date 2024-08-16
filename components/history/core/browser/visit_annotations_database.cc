// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visit_annotations_database.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/transaction.h"

namespace history {

namespace {

#define HISTORY_CONTENT_ANNOTATIONS_ROW_FIELDS                        \
  " visit_id,visibility_score,categories,page_topics_model_version,"  \
  "annotation_flags,entities,related_searches,search_normalized_url," \
  "search_terms,alternative_title,page_language,password_state,"      \
  "has_url_keyed_image "
#define HISTORY_CONTEXT_ANNOTATIONS_ROW_FIELDS                        \
  " visit_id,context_annotation_flags,duration_since_last_visit,"     \
  "page_end_reason,total_foreground_duration,browser_type,window_id," \
  "tab_id,task_id,root_task_id,parent_task_id,response_code "
#define HISTORY_CLUSTER_ROW_FIELDS                                    \
  " cluster_id,should_show_on_prominent_ui_surfaces,label,raw_label," \
  "triggerability_calculated,originator_cache_guid,originator_cluster_id "
#define HISTORY_CLUSTER_VISIT_ROW_FIELDS                          \
  " cluster_id,visit_id,score,engagement_score,url_for_deduping," \
  "normalized_url,url_for_display,interaction_state "

VisitContextAnnotations::BrowserType BrowserTypeFromInt(int type) {
  VisitContextAnnotations::BrowserType converted =
      static_cast<VisitContextAnnotations::BrowserType>(type);
  // Verify that `converted` is actually a valid enum value.
  switch (converted) {
    case VisitContextAnnotations::BrowserType::kUnknown:
    case VisitContextAnnotations::BrowserType::kTabbed:
    case VisitContextAnnotations::BrowserType::kPopup:
    case VisitContextAnnotations::BrowserType::kCustomTab:
    case VisitContextAnnotations::BrowserType::kAuthTab:
      return converted;
  }
  // If the `type` wasn't actually a valid BrowserType value (e.g. due to DB
  // corruption), return `kUnknown` to be safe.
  return VisitContextAnnotations::BrowserType::kUnknown;
}

int BrowserTypeToInt(VisitContextAnnotations::BrowserType type) {
  DCHECK_EQ(BrowserTypeFromInt(static_cast<int>(type)), type);
  return static_cast<int>(type);
}

VisitContentAnnotations::PasswordState PasswordStateFromInt(int state) {
  VisitContentAnnotations::PasswordState converted =
      static_cast<VisitContentAnnotations::PasswordState>(state);
  // Verify that `converted` is actually a valid enum value.
  switch (converted) {
    case VisitContentAnnotations::PasswordState::kUnknown:
    case VisitContentAnnotations::PasswordState::kNoPasswordField:
    case VisitContentAnnotations::PasswordState::kHasPasswordField:
      return converted;
  }
  // If the `state` wasn't actually a valid PasswordState value (e.g. due to DB
  // corruption), return `kUnknown` to be safe.
  return VisitContentAnnotations::PasswordState::kUnknown;
}

int PasswordStateToInt(VisitContentAnnotations::PasswordState state) {
  DCHECK_EQ(PasswordStateFromInt(static_cast<int>(state)), state);
  return static_cast<int>(state);
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
  int64_t flags = 0;
  if (context_annotations.omnibox_url_copied)
    flags |= static_cast<uint64_t>(ContextAnnotationFlags::kOmniboxUrlCopied);
  if (context_annotations.is_existing_part_of_tab_group) {
    flags |= static_cast<uint64_t>(
        ContextAnnotationFlags::kIsExistingPartOfTabGroup);
  }
  if (context_annotations.is_placed_in_tab_group)
    flags |= static_cast<uint64_t>(ContextAnnotationFlags::kIsPlacedInTabGroup);
  if (context_annotations.is_existing_bookmark)
    flags |= static_cast<uint64_t>(ContextAnnotationFlags::kIsExistingBookmark);
  if (context_annotations.is_new_bookmark)
    flags |= static_cast<uint64_t>(ContextAnnotationFlags::kIsNewBookmark);
  if (context_annotations.is_ntp_custom_link)
    flags |= static_cast<uint64_t>(ContextAnnotationFlags::kIsNtpCustomLink);
  return flags;
}

VisitContextAnnotations ConstructContextAnnotationsWithFlags(
    int64_t flags,
    base::TimeDelta duration_since_last_visit,
    int page_end_reason,
    base::TimeDelta total_foreground_duration,
    int browser_type,
    SessionID window_id,
    SessionID tab_id,
    int64_t task_id,
    int64_t root_task_id,
    int64_t parent_task_id,
    int response_code) {
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
  context_annotations.total_foreground_duration = total_foreground_duration;
  context_annotations.on_visit.browser_type = BrowserTypeFromInt(browser_type);
  context_annotations.on_visit.window_id = window_id;
  context_annotations.on_visit.tab_id = tab_id;
  context_annotations.on_visit.task_id = task_id;
  context_annotations.on_visit.root_task_id = root_task_id;
  context_annotations.on_visit.parent_task_id = parent_task_id;
  context_annotations.on_visit.response_code = response_code;
  return context_annotations;
}

ClusterVisit::InteractionState InteractionStateFromInt(int state) {
  ClusterVisit::InteractionState converted =
      static_cast<ClusterVisit::InteractionState>(state);
  // Verify that `converted` is actually a valid enum value.
  switch (converted) {
    case ClusterVisit::InteractionState::kDefault:
    case ClusterVisit::InteractionState::kHidden:
    case ClusterVisit::InteractionState::kDone:
      return converted;
  }
  // If the `state` wasn't actually a valid, return `kDefault` to be safe.
  return ClusterVisit::InteractionState::kDefault;
}

}  // namespace

VisitAnnotationsDatabase::VisitAnnotationsDatabase() = default;
VisitAnnotationsDatabase::~VisitAnnotationsDatabase() = default;

bool VisitAnnotationsDatabase::InitVisitAnnotationsTables() {
  // Content Annotations table.
  if (!GetDB().Execute("CREATE TABLE IF NOT EXISTS content_annotations("
                       "visit_id INTEGER PRIMARY KEY,"
                       "visibility_score NUMERIC,"
                       "floc_protected_score NUMERIC,"
                       "categories VARCHAR,"
                       "page_topics_model_version INTEGER,"
                       "annotation_flags INTEGER NOT NULL,"
                       "entities VARCHAR,"
                       "related_searches VARCHAR,"
                       "search_normalized_url VARCHAR,"
                       "search_terms LONGVARCHAR,"
                       "alternative_title VARCHAR,"
                       "page_language VARCHAR,"
                       "password_state INTEGER DEFAULT 0 NOT NULL,"
                       "has_url_keyed_image BOOLEAN NOT NULL)")) {
    return false;
  }

  // See `VisitContextAnnotations` for details about these fields.
  if (!GetDB().Execute("CREATE TABLE IF NOT EXISTS context_annotations("
                       "visit_id INTEGER PRIMARY KEY,"
                       "context_annotation_flags INTEGER NOT NULL,"
                       "duration_since_last_visit INTEGER,"
                       "page_end_reason INTEGER,"
                       "total_foreground_duration INTEGER,"
                       "browser_type INTEGER DEFAULT 0 NOT NULL,"
                       "window_id INTEGER DEFAULT -1 NOT NULL,"
                       "tab_id INTEGER DEFAULT -1 NOT NULL,"
                       "task_id INTEGER DEFAULT -1 NOT NULL,"
                       "root_task_id INTEGER DEFAULT -1 NOT NULL,"
                       "parent_task_id INTEGER DEFAULT -1 NOT NULL,"
                       "response_code INTEGER DEFAULT 0 NOT NULL)")) {
    return false;
  }

  if (!CreateClustersTable())
    return false;

  // Represents the many-to-many relationship of `Cluster`s and `Visit`s.
  // `score` here is unique to the visit/cluster combination; i.e. the same
  // visit in another cluster or another visit in the same cluster may have
  // different scores.
  if (!CreateClustersAndVisitsTableAndIndex())
    return false;

  // Represents the one-to-many relationship of `Cluster`s and
  // `ClusterKeywordData`s.
  if (!GetDB().Execute("CREATE TABLE IF NOT EXISTS cluster_keywords("
                       "cluster_id INTEGER NOT NULL,"
                       "keyword VARCHAR NOT NULL,"
                       "type INTEGER NOT NULL,"
                       "score NUMERIC NOT NULL,"
                       "collections VARCHAR NOT NULL)")) {
    return false;
  }

  // Index for `cluster_keywords` table.
  if (!GetDB().Execute(
          "CREATE INDEX IF NOT EXISTS cluster_keywords_cluster_id_index ON "
          "cluster_keywords(cluster_id)")) {
    return false;
  }

  // Represents the one-to-many relationship of `ClusterVisit`s and its
  // duplicates: `DuplicateClusterVisit`s.
  if (!GetDB().Execute("CREATE TABLE IF NOT EXISTS cluster_visit_duplicates("
                       "visit_id INTEGER NOT NULL,"
                       "duplicate_visit_id INTEGER NOT NULL,"
                       "PRIMARY KEY(visit_id,duplicate_visit_id))"
                       "WITHOUT ROWID")) {
    return false;
  }

  return true;
}

bool VisitAnnotationsDatabase::DropVisitAnnotationsTables() {
  // Dropping the tables will implicitly delete the indices.
  return GetDB().Execute("DROP TABLE content_annotations") &&
         GetDB().Execute("DROP TABLE context_annotations") &&
         GetDB().Execute("DROP TABLE clusters") &&
         GetDB().Execute("DROP TABLE clusters_and_visits") &&
         GetDB().Execute("DROP TABLE cluster_keywords") &&
         GetDB().Execute("DROP TABLE cluster_visit_duplicates");
}

void VisitAnnotationsDatabase::AddContentAnnotationsForVisit(
    VisitID visit_id,
    const VisitContentAnnotations& visit_content_annotations) {
  DCHECK_GT(visit_id, 0);
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO content_annotations(" HISTORY_CONTENT_ANNOTATIONS_ROW_FIELDS
      ")VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  statement.BindInt64(0, visit_id);
  statement.BindDouble(
      1, static_cast<double>(
             visit_content_annotations.model_annotations.visibility_score));
  statement.BindString(
      2, ConvertCategoriesToStringColumn(
             visit_content_annotations.model_annotations.categories));
  statement.BindInt64(
      3, visit_content_annotations.model_annotations.page_topics_model_version);
  statement.BindInt64(4, visit_content_annotations.annotation_flags);
  statement.BindString(
      5, ConvertCategoriesToStringColumn(
             visit_content_annotations.model_annotations.entities));
  statement.BindString(
      6, SerializeToStringColumn(visit_content_annotations.related_searches));
  statement.BindString(7,
                       visit_content_annotations.search_normalized_url.spec());
  statement.BindString16(8, visit_content_annotations.search_terms);
  statement.BindString(9, visit_content_annotations.alternative_title);
  statement.BindString(10, visit_content_annotations.page_language);
  statement.BindInt(
      11, PasswordStateToInt(visit_content_annotations.password_state));
  statement.BindBool(12, visit_content_annotations.has_url_keyed_image);

  if (!statement.Run()) {
    DVLOG(0) << "Failed to execute 'content_annotations' insert statement:  "
             << "visit_id = " << visit_id;
  }
}

void VisitAnnotationsDatabase::AddContextAnnotationsForVisit(
    VisitID visit_id,
    const VisitContextAnnotations& visit_context_annotations) {
  DCHECK_GT(visit_id, 0);
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO context_annotations(" HISTORY_CONTEXT_ANNOTATIONS_ROW_FIELDS
      ")VALUES(?,?,?,?,?,?,?,?,?,?,?,?)"));
  statement.BindInt64(0, visit_id);
  statement.BindInt64(1, ContextAnnotationsToFlags(visit_context_annotations));
  statement.BindInt64(
      2, visit_context_annotations.duration_since_last_visit.InMicroseconds());
  statement.BindInt(3, visit_context_annotations.page_end_reason);
  statement.BindInt64(
      4, visit_context_annotations.total_foreground_duration.InMicroseconds());
  statement.BindInt(
      5, BrowserTypeToInt(visit_context_annotations.on_visit.browser_type));
  statement.BindInt(6, visit_context_annotations.on_visit.window_id.id());
  statement.BindInt(7, visit_context_annotations.on_visit.tab_id.id());
  statement.BindInt64(8, visit_context_annotations.on_visit.task_id);
  statement.BindInt64(9, visit_context_annotations.on_visit.root_task_id);
  statement.BindInt64(10, visit_context_annotations.on_visit.parent_task_id);
  statement.BindInt(11, visit_context_annotations.on_visit.response_code);

  if (!statement.Run()) {
    DVLOG(0)
        << "Failed to execute visit 'context_annotations' insert statement:  "
        << "visit_id = " << visit_id;
  }
}

void VisitAnnotationsDatabase::UpdateContentAnnotationsForVisit(
    VisitID visit_id,
    const VisitContentAnnotations& visit_content_annotations) {
  DCHECK_GT(visit_id, 0);
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE content_annotations SET "
      "visibility_score=?,categories=?,"
      "page_topics_model_version=?,"
      "annotation_flags=?,entities=?,"
      "related_searches=?,search_normalized_url=?,search_terms=?,"
      "alternative_title=?,has_url_keyed_image=?"
      "WHERE visit_id=?"));
  statement.BindDouble(
      0, static_cast<double>(
             visit_content_annotations.model_annotations.visibility_score));
  statement.BindString(
      1, ConvertCategoriesToStringColumn(
             visit_content_annotations.model_annotations.categories));
  statement.BindInt64(
      2, visit_content_annotations.model_annotations.page_topics_model_version);
  statement.BindInt64(3, visit_content_annotations.annotation_flags);
  statement.BindString(
      4, ConvertCategoriesToStringColumn(
             visit_content_annotations.model_annotations.entities));
  statement.BindString(
      5, SerializeToStringColumn(visit_content_annotations.related_searches));
  statement.BindString(6,
                       visit_content_annotations.search_normalized_url.spec());
  statement.BindString16(7, visit_content_annotations.search_terms);
  statement.BindString(8, visit_content_annotations.alternative_title);
  statement.BindBool(9, visit_content_annotations.has_url_keyed_image);
  statement.BindInt64(10, visit_id);

  if (!statement.Run()) {
    DVLOG(0)
        << "Failed to execute visit 'content_annotations' update statement:  "
        << "visit_id = " << visit_id;
  }
}

void VisitAnnotationsDatabase::UpdateContextAnnotationsForVisit(
    VisitID visit_id,
    const VisitContextAnnotations& visit_context_annotations) {
  DCHECK_GT(visit_id, 0);
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "UPDATE context_annotations SET "
                                 "context_annotation_flags=?, "
                                 "duration_since_last_visit=?, "
                                 "page_end_reason=?, "
                                 "total_foreground_duration=?, "
                                 "browser_type=?, "
                                 "window_id=?, "
                                 "tab_id=?, "
                                 "task_id=?, "
                                 "root_task_id=?, "
                                 "parent_task_id=?, "
                                 "response_code=? "
                                 "WHERE visit_id=?"));
  statement.BindInt64(0, ContextAnnotationsToFlags(visit_context_annotations));
  statement.BindInt64(
      1, visit_context_annotations.duration_since_last_visit.InMicroseconds());
  statement.BindInt(2, visit_context_annotations.page_end_reason);
  statement.BindInt64(
      3, visit_context_annotations.total_foreground_duration.InMicroseconds());
  statement.BindInt(
      4, BrowserTypeToInt(visit_context_annotations.on_visit.browser_type));
  statement.BindInt(5, visit_context_annotations.on_visit.window_id.id());
  statement.BindInt(6, visit_context_annotations.on_visit.tab_id.id());
  statement.BindInt64(7, visit_context_annotations.on_visit.task_id);
  statement.BindInt64(8, visit_context_annotations.on_visit.root_task_id);
  statement.BindInt64(9, visit_context_annotations.on_visit.parent_task_id);
  statement.BindInt64(10, visit_context_annotations.on_visit.response_code);
  statement.BindInt64(11, visit_id);

  if (!statement.Run()) {
    DVLOG(0)
        << "Failed to execute visit 'context_annotations' update statement:  "
        << "visit_id = " << visit_id;
  }
}

bool VisitAnnotationsDatabase::GetContextAnnotationsForVisit(
    VisitID visit_id,
    VisitContextAnnotations* out_context_annotations) {
  DCHECK_GT(visit_id, 0);
  DCHECK(out_context_annotations);

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" HISTORY_CONTEXT_ANNOTATIONS_ROW_FIELDS
                     "FROM context_annotations WHERE visit_id=?"));
  statement.BindInt64(0, visit_id);

  if (!statement.Step())
    return false;

  VisitID received_visit_id = statement.ColumnInt64(0);
  DCHECK_EQ(visit_id, received_visit_id);

  // TODO(tommycli): Make sure ConstructContextAnnotationsWithFlags validates
  //  the column values against potential disk corruption, and add tests.
  // The `VisitID` in column 0 is intentionally ignored, as it's not part of
  // `VisitContextAnnotations`.
  *out_context_annotations = ConstructContextAnnotationsWithFlags(
      statement.ColumnInt64(1), base::Microseconds(statement.ColumnInt64(2)),
      statement.ColumnInt(3), base::Microseconds(statement.ColumnInt64(4)),
      statement.ColumnInt(5),
      SessionID::FromSerializedValue(statement.ColumnInt(6)),
      SessionID::FromSerializedValue(statement.ColumnInt(7)),
      statement.ColumnInt64(8), statement.ColumnInt64(9),
      statement.ColumnInt64(10), statement.ColumnInt(11));
  return true;
}

bool VisitAnnotationsDatabase::GetContentAnnotationsForVisit(
    VisitID visit_id,
    VisitContentAnnotations* out_content_annotations) {
  DCHECK_GT(visit_id, 0);
  DCHECK(out_content_annotations);

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" HISTORY_CONTENT_ANNOTATIONS_ROW_FIELDS
                     "FROM content_annotations WHERE visit_id=?"));
  statement.BindInt64(0, visit_id);

  if (!statement.Step())
    return false;

  VisitID received_visit_id = statement.ColumnInt64(0);
  DCHECK_EQ(visit_id, received_visit_id);

  out_content_annotations->model_annotations.visibility_score =
      static_cast<float>(statement.ColumnDouble(1));
  out_content_annotations->model_annotations.categories =
      GetCategoriesFromStringColumn(statement.ColumnString(2));
  out_content_annotations->model_annotations.page_topics_model_version =
      statement.ColumnInt64(3);
  out_content_annotations->annotation_flags = statement.ColumnInt64(4);
  out_content_annotations->model_annotations.entities =
      GetCategoriesFromStringColumn(statement.ColumnString(5));
  out_content_annotations->related_searches =
      DeserializeFromStringColumn(statement.ColumnString(6));
  out_content_annotations->search_normalized_url =
      GURL(statement.ColumnString(7));
  out_content_annotations->search_terms = statement.ColumnString16(8);
  out_content_annotations->alternative_title = statement.ColumnString(9);
  out_content_annotations->page_language = statement.ColumnString(10);
  out_content_annotations->password_state =
      PasswordStateFromInt(statement.ColumnInt(11));
  out_content_annotations->has_url_keyed_image = statement.ColumnBool(12);
  return true;
}

void VisitAnnotationsDatabase::DeleteAnnotationsForVisit(VisitID visit_id) {
  DCHECK_GT(visit_id, 0);
  sql::Statement statement;

  statement.Assign(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM content_annotations WHERE visit_id=?"));
  statement.BindInt64(0, visit_id);
  if (!statement.Run()) {
    DVLOG(0) << "Failed to execute content_annotations delete statement:  "
             << "visit_id = " << visit_id;
  }

  statement.Assign(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM context_annotations WHERE visit_id=?"));
  statement.BindInt64(0, visit_id);
  if (!statement.Run()) {
    DVLOG(0) << "Failed to execute context_annotations delete statement:  "
             << "visit_id = " << visit_id;
  }

  statement.Assign(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "DELETE FROM cluster_visit_duplicates "
                                 "WHERE visit_id=? OR duplicate_visit_id=?"));
  statement.BindInt64(0, visit_id);
  statement.BindInt64(1, visit_id);
  if (!statement.Run()) {
    DVLOG(0) << "Failed to execute cluster_visit_duplicates delete statement:  "
             << "visit_id = " << visit_id;
  }

  auto cluster_id = GetClusterIdContainingVisit(visit_id);
  if (cluster_id > 0 && GetVisitIdsInCluster(cluster_id).size() == 1)
    DeleteClusters({cluster_id});

  statement.Assign(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM clusters_and_visits WHERE visit_id=?"));
  statement.BindInt64(0, visit_id);
  if (!statement.Run()) {
    DVLOG(0) << "Failed to execute clusters_and_visits delete statement:  "
             << "visit_id = " << visit_id;
  }
}

void VisitAnnotationsDatabase::AddClusters(
    const std::vector<Cluster>& clusters) {
  if (clusters.empty())
    return;

  sql::Statement clusters_statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO clusters"
                                 "(should_show_on_prominent_ui_surfaces,label,"
                                 "raw_label,triggerability_calculated,"
                                 "originator_cache_guid,originator_cluster_id)"
                                 "VALUES(?,?,?,?,?,?)"));
  sql::Statement clusters_and_visits_statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO clusters_and_visits"
                                 "(" HISTORY_CLUSTER_VISIT_ROW_FIELDS ")"
                                 "VALUES(?,?,?,?,?,?,?,?)"));
  sql::Statement cluster_keywords_statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO cluster_keywords"
                                 "(cluster_id,keyword,type,score,collections)"
                                 "VALUES(?,?,?,?,?)"));
  // INSERT OR IGNORE, because these rows are not keyed on `cluster_id`, so it's
  // difficult to guarantee complete cleanup. https://crbug.com/1383274
  sql::Statement cluster_visit_duplicates_statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR IGNORE INTO cluster_visit_duplicates"
      "(visit_id,duplicate_visit_id)"
      "VALUES(?,?)"));

  for (const auto& cluster : clusters) {
    if (cluster.visits.empty())
      continue;

    // Insert the cluster into 'clusters'.
    clusters_statement.Reset(true);
    clusters_statement.BindBool(0,
                                cluster.should_show_on_prominent_ui_surfaces);
    clusters_statement.BindString16(1, cluster.label.value_or(u""));
    clusters_statement.BindString16(2, cluster.raw_label.value_or(u""));
    clusters_statement.BindBool(3, cluster.triggerability_calculated);
    clusters_statement.BindString(4, cluster.originator_cache_guid);
    clusters_statement.BindInt64(5, cluster.originator_cluster_id);
    if (!clusters_statement.Run()) {
      DVLOG(0) << "Failed to execute 'clusters' insert statement";
      continue;
    }
    const int64_t cluster_id = GetDB().GetLastInsertRowId();
    DCHECK_GT(cluster_id, 0);

    // Insert each visit into 'clusters_and_visits'.
    base::ranges::for_each(cluster.visits, [&](const auto& cluster_visit) {
      const auto visit_id = cluster_visit.annotated_visit.visit_row.visit_id;
      DCHECK_GT(visit_id, 0);
      clusters_and_visits_statement.Reset(true);
      clusters_and_visits_statement.BindInt64(0, cluster_id);
      clusters_and_visits_statement.BindInt64(1, visit_id);
      clusters_and_visits_statement.BindDouble(2, cluster_visit.score);
      clusters_and_visits_statement.BindDouble(3,
                                               cluster_visit.engagement_score);
      clusters_and_visits_statement.BindString(
          4, cluster_visit.url_for_deduping.spec());
      clusters_and_visits_statement.BindString(
          5, cluster_visit.normalized_url.spec());
      clusters_and_visits_statement.BindString16(6,
                                                 cluster_visit.url_for_display);
      clusters_and_visits_statement.BindInt(
          7,
          ClusterVisit::InteractionStateToInt(cluster_visit.interaction_state));
      if (!clusters_and_visits_statement.Run()) {
        DVLOG(0)
            << "Failed to execute 'clusters_and_visits' insert statement:  "
            << "cluster_id = " << cluster_id << ", visit_id = " << visit_id;
      }

      // Insert each `ClusterVisit`'s duplicate visits into
      // 'cluster_visit_duplicates_statement'.
      for (const auto& duplicate_visit : cluster_visit.duplicate_visits) {
        DCHECK_GT(duplicate_visit.visit_id, 0);
        cluster_visit_duplicates_statement.Reset(true);
        cluster_visit_duplicates_statement.BindInt64(0, visit_id);
        cluster_visit_duplicates_statement.BindInt64(1,
                                                     duplicate_visit.visit_id);
        if (!cluster_visit_duplicates_statement.Run()) {
          DVLOG(0) << "Failed to execute 'cluster_visit_duplicates' insert "
                      "statement:  "
                   << "cluster_id = " << cluster_id
                   << ", visit_id = " << visit_id
                   << ", duplicate_visit_id = " << duplicate_visit.visit_id;
        }
      }
    });

    // Insert each keyword into 'cluster_keywords'.
    for (const auto& [keyword, keyword_data] : cluster.keyword_to_data_map) {
      cluster_keywords_statement.Reset(true);
      cluster_keywords_statement.BindInt64(0, cluster_id);
      cluster_keywords_statement.BindString16(1, keyword);
      cluster_keywords_statement.BindInt(2, keyword_data.type);
      cluster_keywords_statement.BindDouble(3, keyword_data.score);
      cluster_keywords_statement.BindString(4, "");
      if (!cluster_keywords_statement.Run()) {
        DVLOG(0) << "Failed to execute 'cluster_keywords' insert statement:  "
                 << "cluster_id = " << cluster_id << ", keyword = " << keyword;
      }
    }
  }
}

int64_t VisitAnnotationsDatabase::ReserveNextClusterId(
    const std::string& originator_cache_guid,
    int64_t originator_cluster_id) {
  sql::Statement clusters_statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO clusters"
                                 "(should_show_on_prominent_ui_surfaces,label,"
                                 "raw_label,triggerability_calculated,"
                                 "originator_cache_guid,originator_cluster_id)"
                                 "VALUES(?,?,?,?,?,?)"));
  clusters_statement.BindBool(0, false);
  clusters_statement.BindString16(1, u"");
  clusters_statement.BindString16(2, u"");
  clusters_statement.BindBool(3, false);
  clusters_statement.BindString(4, originator_cache_guid);
  clusters_statement.BindInt64(5, originator_cluster_id);
  if (!clusters_statement.Run()) {
    DVLOG(0) << "Failed to execute 'clusters' insert statement";
  }
  return GetDB().GetLastInsertRowId();
}

void VisitAnnotationsDatabase::AddVisitsToCluster(
    int64_t cluster_id,
    const std::vector<ClusterVisit>& visits) {
  DCHECK_GT(cluster_id, 0);
  sql::Statement clusters_and_visits_statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO clusters_and_visits"
                                 "(" HISTORY_CLUSTER_VISIT_ROW_FIELDS ")"
                                 "VALUES(?,?,?,?,?,?,?,?)"));

  // Insert each visit into 'clusters_and_visits'.
  base::ranges::for_each(visits, [&](const auto& visit) {
    DCHECK_GT(visit.annotated_visit.visit_row.visit_id, 0);
    clusters_and_visits_statement.Reset(true);
    clusters_and_visits_statement.BindInt64(0, cluster_id);
    clusters_and_visits_statement.BindInt64(
        1, visit.annotated_visit.visit_row.visit_id);
    // Tentatively score everything as 1.0.
    clusters_and_visits_statement.BindDouble(2, 1.0);
    clusters_and_visits_statement.BindDouble(3, visit.engagement_score);
    clusters_and_visits_statement.BindString(4, visit.url_for_deduping.spec());
    clusters_and_visits_statement.BindString(5, visit.normalized_url.spec());
    clusters_and_visits_statement.BindString16(6, visit.url_for_display);
    clusters_and_visits_statement.BindInt(
        7, ClusterVisit::InteractionStateToInt(visit.interaction_state));
    if (!clusters_and_visits_statement.Run()) {
      DVLOG(0) << "Failed to execute 'clusters_and_visits' insert statement:  "
               << "cluster_id = " << cluster_id
               << ", visit_id = " << visit.annotated_visit.visit_row.visit_id;
    }
  });
}

void VisitAnnotationsDatabase::UpdateClusterTriggerability(
    const std::vector<history::Cluster>& clusters) {
  sql::Statement clusters_statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE clusters "
      "SET "
      "should_show_on_prominent_ui_surfaces=?,label=?,"
      "raw_label=?,triggerability_calculated=? "
      "WHERE cluster_id=?"));

  sql::Statement delete_cluster_keywords_statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM cluster_keywords WHERE cluster_id=?"));

  sql::Statement cluster_keywords_statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO cluster_keywords"
                                 "(cluster_id,keyword,type,score,collections)"
                                 "VALUES(?,?,?,?,?)"));

  sql::Statement update_cluster_visit_scores_statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "UPDATE clusters_and_visits SET score=? WHERE "
                                 "cluster_id=? AND visit_id=?"));

  // INSERT OR IGNORE, because these rows are not keyed on `cluster_id`, so it's
  // difficult to guarantee complete cleanup. https://crbug.com/1383274
  sql::Statement cluster_visit_duplicates_statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR IGNORE INTO cluster_visit_duplicates"
      "(visit_id,duplicate_visit_id)"
      "VALUES(?,?)"));

  base::ranges::for_each(clusters, [&](const auto& cluster) {
    DCHECK_GT(cluster.cluster_id, 0);

    // Update cluster visibility.
    clusters_statement.Reset(true);
    clusters_statement.BindBool(0,
                                cluster.should_show_on_prominent_ui_surfaces);
    clusters_statement.BindString16(1, cluster.label.value_or(u""));
    clusters_statement.BindString16(2, cluster.raw_label.value_or(u""));
    clusters_statement.BindBool(3, cluster.triggerability_calculated);
    clusters_statement.BindInt64(4, cluster.cluster_id);
    if (!clusters_statement.Run()) {
      DVLOG(0) << "Failed to execute clusters update statement:  "
               << "cluster_id = " << cluster.cluster_id;
    }

    // Delete all previously persisted keywords.
    delete_cluster_keywords_statement.Reset(true);
    delete_cluster_keywords_statement.BindInt64(0, cluster.cluster_id);
    if (!delete_cluster_keywords_statement.Run()) {
      DVLOG(0) << "Failed to execute 'cluster_keywords' delete statement in "
                  "`UpdateClusterTriggerability()`:  cluster_id = "
               << cluster.cluster_id;
    }

    // Add each keyword into 'cluster_keywords'.
    for (const auto& [keyword, keyword_data] : cluster.keyword_to_data_map) {
      cluster_keywords_statement.Reset(true);
      cluster_keywords_statement.BindInt64(0, cluster.cluster_id);
      cluster_keywords_statement.BindString16(1, keyword);
      cluster_keywords_statement.BindInt(2, keyword_data.type);
      cluster_keywords_statement.BindDouble(3, keyword_data.score);
      cluster_keywords_statement.BindString(4, "");
      if (!cluster_keywords_statement.Run()) {
        DVLOG(0) << "Failed to execute 'cluster_keywords' insert statement in "
                    "`UpdateClusterTriggerability()`:  "
                 << "cluster_id = " << cluster.cluster_id
                 << ", keyword = " << keyword;
      }
    }

    base::ranges::for_each(cluster.visits, [&](const auto& cluster_visit) {
      const auto visit_id = cluster_visit.annotated_visit.visit_row.visit_id;
      DCHECK_GT(visit_id, 0);
      update_cluster_visit_scores_statement.Reset(true);
      update_cluster_visit_scores_statement.BindDouble(0, cluster_visit.score);
      update_cluster_visit_scores_statement.BindInt64(1, cluster.cluster_id);
      update_cluster_visit_scores_statement.BindInt64(2, visit_id);
      if (!update_cluster_visit_scores_statement.Run()) {
        DVLOG(0) << "Failed to execute 'clusters_and_visits' update statement "
                    "in `UpdateClusterTriggerability()`:  "
                 << "cluster_id = " << cluster.cluster_id
                 << ", visit_id = " << visit_id;
      }

      // Insert each `ClusterVisit`'s duplicate visits into
      // 'cluster_visit_duplicates_statement'.
      for (const auto& duplicate_visit : cluster_visit.duplicate_visits) {
        DCHECK_GT(duplicate_visit.visit_id, 0);
        cluster_visit_duplicates_statement.Reset(true);
        cluster_visit_duplicates_statement.BindInt64(0, visit_id);
        cluster_visit_duplicates_statement.BindInt64(1,
                                                     duplicate_visit.visit_id);
        if (!cluster_visit_duplicates_statement.Run()) {
          DVLOG(0) << "Failed to execute 'cluster_visit_duplicates' insert in "
                      "statement in `UpdateClusterTriggerability()`:  "
                   << "cluster_id = " << cluster.cluster_id
                   << ", visit_id = " << visit_id
                   << ", duplicate_visit_id = " << duplicate_visit.visit_id;
        }
      }
    });
  });
}

void VisitAnnotationsDatabase::UpdateClusterVisit(
    int64_t cluster_id,
    const history::ClusterVisit& cluster_visit) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "UPDATE clusters_and_visits "
                                 "SET "
                                 "engagement_score=?,url_for_deduping=?,"
                                 "normalized_url=?,url_for_display=?,"
                                 "interaction_state=? "
                                 "WHERE cluster_id=? AND visit_id=?"));
  statement.BindDouble(0, cluster_visit.engagement_score);
  statement.BindString(1, cluster_visit.url_for_deduping.spec());
  statement.BindString(2, cluster_visit.normalized_url.spec());
  statement.BindString16(3, cluster_visit.url_for_display);
  statement.BindInt(
      4, ClusterVisit::InteractionStateToInt(cluster_visit.interaction_state));
  statement.BindInt64(5, cluster_id);
  statement.BindInt64(6, cluster_visit.annotated_visit.visit_row.visit_id);
  if (!statement.Run()) {
    DVLOG(0) << "Failed to execute 'clusters_and_visits' update statement in "
                "`UpdateClusterVisit()`: "
             << "cluster_id = " << cluster_id << ", visit_id = "
             << cluster_visit.annotated_visit.visit_row.visit_id;
  }
}

Cluster VisitAnnotationsDatabase::GetCluster(int64_t cluster_id) {
  DCHECK_GT(cluster_id, 0);
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT" HISTORY_CLUSTER_ROW_FIELDS "FROM clusters WHERE cluster_id=?"));
  statement.BindInt64(0, cluster_id);

  if (!statement.Step())
    return {};

  VisitID received_cluster_id = statement.ColumnInt64(0);
  DCHECK_EQ(cluster_id, received_cluster_id);

  // The `VisitID` in column 0 is intentionally ignored, as it's not part of
  // `VisitContextAnnotations`.
  Cluster cluster;
  cluster.cluster_id = received_cluster_id;
  cluster.from_persistence = true;
  cluster.should_show_on_prominent_ui_surfaces = statement.ColumnBool(1);
  // The DB can't represent `nullopt` labels, so they're persisted as u"" but
  // retrieved as `nullopt` for consistency with their original values and the
  // consumer expectations.
  // TODO(manukh): Look into returning u"" instead of `nullopt` in the
  //  clustering code, and likewise expect u"" instead of `nullopt` in the
  //  clustering UI code.
  cluster.label = statement.ColumnString16(2);
  if (cluster.label->empty())
    cluster.label = std::nullopt;
  cluster.raw_label = statement.ColumnString16(3);
  if (cluster.raw_label->empty())
    cluster.raw_label = std::nullopt;
  cluster.triggerability_calculated = statement.ColumnBool(4);
  cluster.originator_cache_guid = statement.ColumnString(5);
  cluster.originator_cluster_id = statement.ColumnInt64(6);
  return cluster;
}

std::vector<int64_t> VisitAnnotationsDatabase::GetMostRecentClusterIds(
    base::Time inclusive_min_time,
    base::Time exclusive_max_time,
    int max_clusters) {
  DCHECK_GT(max_clusters, 0);
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT cluster_id "
      "FROM clusters_and_visits "
      "JOIN visits ON visit_id=id "
      "GROUP BY cluster_id "
      "HAVING MAX(visit_time)>=? AND MAX(visit_time)<? "
      "ORDER BY MAX(visit_time) DESC "
      "LIMIT ?"));
  statement.BindTime(0, inclusive_min_time);
  statement.BindTime(1, exclusive_max_time);
  statement.BindInt(2, max_clusters);

  std::vector<int64_t> cluster_ids;
  while (statement.Step())
    cluster_ids.push_back(statement.ColumnInt64(0));
  return cluster_ids;
}

std::vector<VisitID> VisitAnnotationsDatabase::GetVisitIdsInCluster(
    int64_t cluster_id) {
  DCHECK_GT(cluster_id, 0);
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT visit_id "
                                 "FROM clusters_and_visits "
                                 "WHERE cluster_id=? "
                                 "ORDER BY score DESC,visit_id DESC"));
  statement.BindInt64(0, cluster_id);

  std::vector<VisitID> visit_ids;
  while (statement.Step())
    visit_ids.push_back(statement.ColumnInt64(0));
  return visit_ids;
}

ClusterVisit VisitAnnotationsDatabase::GetClusterVisit(VisitID visit_id) {
  DCHECK_GT(visit_id, 0);
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" HISTORY_CLUSTER_VISIT_ROW_FIELDS
                     "FROM clusters_and_visits WHERE visit_id=?"));
  statement.BindInt64(0, visit_id);

  if (!statement.Step())
    return {};

  VisitID received_visit_id = statement.ColumnInt64(1);
  DCHECK_EQ(visit_id, received_visit_id);

  // The `VisitID` in column 0 is intentionally ignored, as it's not part of
  // `VisitContextAnnotations`.
  ClusterVisit cluster_visit;
  cluster_visit.annotated_visit.visit_row.visit_id = received_visit_id;
  cluster_visit.score = static_cast<float>(statement.ColumnDouble(2));
  cluster_visit.engagement_score =
      static_cast<float>(statement.ColumnDouble(3));
  cluster_visit.url_for_deduping = GURL(statement.ColumnString(4));
  cluster_visit.normalized_url = GURL(statement.ColumnString(5));
  cluster_visit.url_for_display = statement.ColumnString16(6);
  cluster_visit.interaction_state =
      InteractionStateFromInt(statement.ColumnInt(7));
  return cluster_visit;
}

std::vector<VisitID>
VisitAnnotationsDatabase::GetDuplicateClusterVisitIdsForClusterVisit(
    int64_t visit_id) {
  DCHECK_GT(visit_id, 0);
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT duplicate_visit_id "
                                 "FROM cluster_visit_duplicates "
                                 "WHERE visit_id=?"));
  statement.BindInt64(0, visit_id);

  std::vector<VisitID> visit_ids;
  while (statement.Step())
    visit_ids.push_back(statement.ColumnInt64(0));
  return visit_ids;
}

int64_t VisitAnnotationsDatabase::GetClusterIdContainingVisit(
    VisitID visit_id) {
  DCHECK_GT(visit_id, 0);
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT cluster_id "
                                 "FROM clusters_and_visits "
                                 "WHERE visit_id=? "
                                 "LIMIT 1"));
  statement.BindInt64(0, visit_id);
  if (statement.Step())
    return statement.ColumnInt64(0);
  return 0;
}

int64_t VisitAnnotationsDatabase::GetClusterIdForSyncedDetails(
    const std::string& originator_cache_guid,
    int64_t originator_cluster_id) {
  DCHECK(!originator_cache_guid.empty());
  DCHECK_GT(originator_cluster_id, 0);

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT cluster_id "
      "FROM clusters "
      "WHERE originator_cache_guid=? AND originator_cluster_id=? "
      "LIMIT 1"));
  statement.BindString(0, originator_cache_guid);
  statement.BindInt64(1, originator_cluster_id);
  if (statement.Step()) {
    return statement.ColumnInt64(0);
  }
  return 0;
}

base::flat_map<std::u16string, ClusterKeywordData>
VisitAnnotationsDatabase::GetClusterKeywords(int64_t cluster_id) {
  DCHECK_GT(cluster_id, 0);
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT keyword,type,score,collections "
                                 "FROM cluster_keywords "
                                 "WHERE cluster_id=?"));
  statement.BindInt64(0, cluster_id);

  base::flat_map<std::u16string, ClusterKeywordData> keyword_data;
  while (statement.Step()) {
    keyword_data[statement.ColumnString16(0)] = {
        static_cast<ClusterKeywordData::ClusterKeywordType>(
            statement.ColumnInt(1)),
        static_cast<float>(statement.ColumnDouble(2))};
  }
  return keyword_data;
}

void VisitAnnotationsDatabase::HideVisits(
    const std::vector<VisitID>& visit_ids) {
  if (visit_ids.empty())
    return;

  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "UPDATE clusters_and_visits "
                                 "SET score=0 WHERE visit_id=?"));

  for (auto visit_id : visit_ids) {
    statement.Reset(true);
    statement.BindInt64(0, visit_id);
    if (!statement.Run()) {
      DVLOG(0) << "Failed to execute visit hide statement:  "
               << "visit_id = " << visit_id;
    }
  }
}

void VisitAnnotationsDatabase::DeleteClusters(
    const std::vector<int64_t>& cluster_ids) {
  if (cluster_ids.empty())
    return;

  sql::Statement clusters_statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM clusters WHERE cluster_id=?"));

  sql::Statement clusters_and_visits_statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM clusters_and_visits WHERE cluster_id=?"));

  sql::Statement cluster_keywords_statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM cluster_keywords WHERE cluster_id=?"));

  sql::Statement cluster_visit_duplicates_statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "DELETE FROM cluster_visit_duplicates "
                                 "WHERE visit_id=? OR duplicate_visit_id=?"));

  for (auto cluster_id : cluster_ids) {
    clusters_statement.Reset(true);
    clusters_statement.BindInt64(0, cluster_id);
    if (!clusters_statement.Run()) {
      DVLOG(0) << "Failed to execute clusters delete statement:  "
               << "cluster_id = " << cluster_id;
    }

    // Delete all duplicates for these visits, because clusters are recreated.
    // Note that this cleanup implicitly assumes that no two clusters have the
    // same visits inside. In practice, this is true. The previous status-quo
    // was to leave these rows around, but that causes UNIQUE constraint
    // violations. https://crbug.com/1383274
    for (auto visit_id : GetVisitIdsInCluster(cluster_id)) {
      cluster_visit_duplicates_statement.Reset(true);
      cluster_visit_duplicates_statement.BindInt64(0, visit_id);
      cluster_visit_duplicates_statement.BindInt64(1, visit_id);
      if (!cluster_visit_duplicates_statement.Run()) {
        DVLOG(0)
            << "Failed to execute cluster_visit_duplicates delete statement:  "
            << "visit_id = " << visit_id;
      }
    }

    clusters_and_visits_statement.Reset(true);
    clusters_and_visits_statement.BindInt64(0, cluster_id);
    if (!clusters_and_visits_statement.Run()) {
      DVLOG(0) << "Failed to execute clusters_and_visits delete statement:  "
               << "cluster_id = " << cluster_id;
    }

    cluster_keywords_statement.Reset(true);
    cluster_keywords_statement.BindInt64(0, cluster_id);
    if (!cluster_keywords_statement.Run()) {
      DVLOG(0) << "Failed to execute cluster_keywords delete statement:  "
               << "cluster_id = " << cluster_id;
    }
  }
}

void VisitAnnotationsDatabase::UpdateVisitsInteractionState(
    const std::vector<VisitID>& visit_ids,
    ClusterVisit::InteractionState interaction_state) {
  if (visit_ids.empty()) {
    return;
  }

  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "UPDATE clusters_and_visits "
                                 "SET interaction_state=? WHERE visit_id=?"));
  for (auto visit_id : visit_ids) {
    statement.Reset(true);
    statement.BindInt(0,
                      ClusterVisit::InteractionStateToInt(interaction_state));
    statement.BindInt64(1, visit_id);
    if (!statement.Run()) {
      DVLOG(0) << "Failed to execute visit hide statement:  "
               << "visit_id = " << visit_id;
    }
  }
}

bool VisitAnnotationsDatabase::MigrateFlocAllowedToAnnotationsTable() {
  if (!GetDB().DoesTableExist("content_annotations")) {
    NOTREACHED_IN_MIGRATION()
        << " content_annotations table should exist before migration";
    return false;
  }

  // Not all version 43 history has the content_annotations table. So at this
  // point the content_annotations table may already have been initialized with
  // the latest version with an annotation_flags column.
  if (!GetDB().DoesColumnExist("content_annotations", "annotation_flags")) {
    // Add an annotation_flags column to the content_annotations table.
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
  if (!GetDB().Execute("INSERT OR IGNORE INTO content_annotations"
                       "(visit_id,floc_protected_score,categories,"
                       "page_topics_model_version,annotation_flags)"
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

bool VisitAnnotationsDatabase::
    MigrateContentAnnotationsWithoutEntitiesColumn() {
  if (!GetDB().DoesTableExist("content_annotations")) {
    NOTREACHED_IN_MIGRATION()
        << " Content annotations table should exist before migration";
    return false;
  }

  if (GetDB().DoesColumnExist("content_annotations", "entities"))
    return true;

  // Old versions don't have the entities column, we modify the table to add
  // that field.
  return GetDB().Execute(
      "ALTER TABLE content_annotations "
      "ADD COLUMN entities VARCHAR");
}

bool VisitAnnotationsDatabase::
    MigrateContentAnnotationsAddRelatedSearchesColumn() {
  if (!GetDB().DoesTableExist("content_annotations")) {
    NOTREACHED_IN_MIGRATION()
        << " Content annotations table should exist before migration";
    return false;
  }

  if (GetDB().DoesColumnExist("content_annotations", "related_searches"))
    return true;

  // Add the `related_searches` column to the older versions of the table.
  return GetDB().Execute(
      "ALTER TABLE content_annotations "
      "ADD COLUMN related_searches VARCHAR");
}

bool VisitAnnotationsDatabase::MigrateContentAnnotationsAddVisibilityScore() {
  if (!GetDB().DoesTableExist("content_annotations")) {
    NOTREACHED_IN_MIGRATION()
        << " Content annotations table should exist before migration";
    return false;
  }

  if (GetDB().DoesColumnExist("content_annotations", "visibility_score"))
    return true;
  return GetDB().Execute(
      "ALTER TABLE content_annotations "
      "ADD COLUMN visibility_score NUMERIC DEFAULT -1");
}

bool VisitAnnotationsDatabase::
    MigrateContextAnnotationsAddTotalForegroundDuration() {
  if (!GetDB().DoesTableExist("context_annotations")) {
    NOTREACHED_IN_MIGRATION()
        << " Context annotations table should exist before migration";
    return false;
  }

  if (GetDB().DoesColumnExist("context_annotations",
                              "total_foreground_duration"))
    return true;
  // 1000000us = 1s which is the default duration for this DB.
  return GetDB().Execute(
      "ALTER TABLE context_annotations "
      "ADD COLUMN total_foreground_duration NUMERIC DEFAULT -1000000");
}

bool VisitAnnotationsDatabase::MigrateContentAnnotationsAddSearchMetadata() {
  if (!GetDB().DoesTableExist("content_annotations")) {
    NOTREACHED_IN_MIGRATION()
        << " Content annotations table should exist before migration";
    return false;
  }

  if (GetDB().DoesColumnExist("content_annotations", "search_normalized_url") &&
      GetDB().DoesColumnExist("content_annotations", "search_terms")) {
    return true;
  }

  // Add the `search_normalized_url` and `search_terms` columns to the older
  // versions of the table.
  return GetDB().Execute(
      "ALTER TABLE content_annotations "
      "ADD COLUMN search_normalized_url; \n"
      "ALTER TABLE content_annotations ADD COLUMN search_terms LONGVARCHAR");
}

bool VisitAnnotationsDatabase::MigrateContentAnnotationsAddAlternativeTitle() {
  if (!GetDB().DoesTableExist("content_annotations")) {
    NOTREACHED_IN_MIGRATION()
        << "Content annotations table should exist before migration";
    return false;
  }

  if (GetDB().DoesColumnExist("content_annotations", "alternative_title"))
    return true;

  // Add the `alternative_title`column to the older versions of the table.
  return GetDB().Execute(
      "ALTER TABLE content_annotations "
      "ADD COLUMN alternative_title");
}

bool VisitAnnotationsDatabase::MigrateClustersAddColumns() {
  // Don't need to actually copy values from the previous table; it was never
  // populated
  return (!GetDB().DoesTableExist("clusters") ||
          GetDB().Execute("DROP TABLE clusters")) &&
         (!GetDB().DoesTableExist("clusters_and_visits") ||
          GetDB().Execute("DROP TABLE clusters_and_visits")) &&
         CreateClustersTable() && CreateClustersAndVisitsTableAndIndex();
}

bool VisitAnnotationsDatabase::MigrateAnnotationsAddColumnsForSync() {
  if (!GetDB().DoesTableExist("context_annotations")) {
    NOTREACHED_IN_MIGRATION()
        << " Context annotations table should exist before migration";
    return false;
  }

  // Context annotation columns:

  if (!GetDB().DoesColumnExist("context_annotations", "browser_type")) {
    if (!GetDB().Execute(
            "ALTER TABLE context_annotations "
            "ADD COLUMN browser_type INTEGER DEFAULT 0 NOT NULL")) {
      return false;
    }
  }

  if (!GetDB().DoesColumnExist("context_annotations", "window_id")) {
    if (!GetDB().Execute("ALTER TABLE context_annotations "
                         "ADD COLUMN window_id INTEGER DEFAULT -1 NOT NULL")) {
      return false;
    }
  }

  if (!GetDB().DoesColumnExist("context_annotations", "tab_id")) {
    if (!GetDB().Execute("ALTER TABLE context_annotations "
                         "ADD COLUMN tab_id INTEGER DEFAULT -1 NOT NULL")) {
      return false;
    }
  }

  if (!GetDB().DoesColumnExist("context_annotations", "task_id")) {
    if (!GetDB().Execute("ALTER TABLE context_annotations "
                         "ADD COLUMN task_id INTEGER DEFAULT -1 NOT NULL")) {
      return false;
    }
  }

  if (!GetDB().DoesColumnExist("context_annotations", "root_task_id")) {
    if (!GetDB().Execute(
            "ALTER TABLE context_annotations "
            "ADD COLUMN root_task_id INTEGER DEFAULT -1 NOT NULL")) {
      return false;
    }
  }

  if (!GetDB().DoesColumnExist("context_annotations", "parent_task_id")) {
    if (!GetDB().Execute(
            "ALTER TABLE context_annotations "
            "ADD COLUMN parent_task_id INTEGER DEFAULT -1 NOT NULL")) {
      return false;
    }
  }

  if (!GetDB().DoesColumnExist("context_annotations", "response_code")) {
    if (!GetDB().Execute(
            "ALTER TABLE context_annotations "
            "ADD COLUMN response_code INTEGER DEFAULT 0 NOT NULL")) {
      return false;
    }
  }

  // Content annotation columns:

  if (!GetDB().DoesColumnExist("content_annotations", "page_language")) {
    if (!GetDB().Execute("ALTER TABLE content_annotations "
                         "ADD COLUMN page_language VARCHAR")) {
      return false;
    }
  }

  if (!GetDB().DoesColumnExist("content_annotations", "password_state")) {
    if (!GetDB().Execute(
            "ALTER TABLE content_annotations "
            "ADD COLUMN password_state INTEGER DEFAULT 0 NOT NULL")) {
      return false;
    }
  }

  return true;
}

bool VisitAnnotationsDatabase::MigrateClustersAddTriggerabilityCalculated() {
  if (!GetDB().DoesTableExist("clusters")) {
    NOTREACHED_IN_MIGRATION()
        << " Clusters table should exist before migration";
    return false;
  }

  if (GetDB().DoesColumnExist("clusters", "triggerability_calculated")) {
    return true;
  }
  // Set default to true, as clusters added to this table prior to this column
  // getting added are the fully formed clusters rather than just the basic
  // ones.
  return GetDB().Execute(
      "ALTER TABLE clusters "
      "ADD COLUMN triggerability_calculated BOOL DEFAULT TRUE");
}

bool VisitAnnotationsDatabase::
    MigrateClustersAutoincrementIdAndAddOriginatorColumns() {
  if (!GetDB().DoesTableExist("clusters")) {
    NOTREACHED_IN_MIGRATION()
        << " Clusters table should exist before migration";
    return false;
  }

  if (GetDB().DoesColumnExist("clusters", "originator_cache_guid") &&
      GetDB().DoesColumnExist("clusters", "originator_cluster_id") &&
      ClustersTableContainsAutoincrement()) {
    return true;
  }

  sql::Transaction transaction(&GetDB());
  return transaction.Begin() &&
         GetDB().Execute(
             "CREATE TABLE clusters_tmp("
             "cluster_id INTEGER PRIMARY KEY AUTOINCREMENT,"
             "should_show_on_prominent_ui_surfaces BOOLEAN NOT NULL,"
             "label VARCHAR NOT NULL,"
             "raw_label VARCHAR NOT NULL,"
             "triggerability_calculated BOOLEAN NOT NULL,"
             "originator_cache_guid TEXT DEFAULT \"\" NOT NULL,"
             "originator_cluster_id INTEGER DEFAULT 0 NOT NULL)") &&
         GetDB().Execute(
             "INSERT INTO clusters_tmp("
             "cluster_id,should_show_on_prominent_ui_surfaces,label,raw_label,"
             "triggerability_calculated)"
             "SELECT "
             "cluster_id,should_show_on_prominent_ui_surfaces,label,raw_label,"
             "triggerability_calculated FROM clusters") &&
         GetDB().Execute("DROP TABLE clusters") &&
         GetDB().Execute("ALTER TABLE clusters_tmp RENAME TO clusters") &&
         transaction.Commit();
}

bool VisitAnnotationsDatabase::ClustersTableContainsAutoincrement() {
  // sqlite_schema has columns:
  //   type - "index" or "table".
  //   name - name of created element.
  //   tbl_name - name of element, or target table in case of index.
  //   rootpage - root page of the element in database file.
  //   sql - SQL to create the element.
  sql::Statement statement(
      GetDB().GetUniqueStatement("SELECT sql FROM sqlite_schema WHERE type = "
                                 "'table' AND name = 'clusters'"));

  // clusters table does not exist.
  if (!statement.Step()) {
    return false;
  }

  std::string clusters_schema = statement.ColumnString(0);
  // We check if the whole schema contains "AUTOINCREMENT", since
  // "AUTOINCREMENT" only can be used for "INTEGER PRIMARY KEY", so we assume no
  // other columns could contain "AUTOINCREMENT".
  return clusters_schema.find("AUTOINCREMENT") != std::string::npos;
}

bool VisitAnnotationsDatabase::MigrateContentAnnotationsAddHasUrlKeyedImage() {
  if (!GetDB().DoesTableExist("content_annotations")) {
    NOTREACHED_IN_MIGRATION()
        << " Content annotations table should exist before migration";
    return false;
  }

  if (GetDB().DoesColumnExist("content_annotations", "has_url_keyed_image")) {
    return true;
  }
  return GetDB().Execute(
      "ALTER TABLE content_annotations "
      "ADD COLUMN has_url_keyed_image BOOLEAN DEFAULT false NOT NULL");
}

bool VisitAnnotationsDatabase::MigrateClustersAndVisitsAddInteractionState() {
  if (!GetDB().DoesTableExist("clusters_and_visits")) {
    NOTREACHED_IN_MIGRATION()
        << "clusters_and_visits table should exist before migration";
    return false;
  }

  if (GetDB().DoesColumnExist("clusters_and_visits", "interaction_state")) {
    return true;
  }
  return GetDB().Execute(
      "ALTER TABLE clusters_and_visits "
      "ADD COLUMN interaction_state INTEGER DEFAULT 0 NOT NULL");
}

bool VisitAnnotationsDatabase::CreateClustersTable() {
  // The `id` uses AUTOINCREMENT to support Sync. Chrome Sync uses the
  // `id` in conjunction with the Client ID as a unique identifier.
  // If this was not AUTOINCREMENT, deleting a row and creating a new
  // one could reuse the same `id` for an entirely new cluster, which
  // would confuse Sync, as Sync would be unable to distinguish
  // an update from a deletion plus a creation.
  return GetDB().Execute(
      "CREATE TABLE IF NOT EXISTS clusters("
      "cluster_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "should_show_on_prominent_ui_surfaces BOOLEAN NOT NULL,"
      "label VARCHAR NOT NULL,"
      "raw_label VARCHAR NOT NULL,"
      "triggerability_calculated BOOLEAN NOT NULL,"
      "originator_cache_guid TEXT NOT NULL,"
      "originator_cluster_id INTEGER NOT NULL)");
}

bool VisitAnnotationsDatabase::CreateClustersAndVisitsTableAndIndex() {
  return GetDB().Execute(
             "CREATE TABLE IF NOT EXISTS clusters_and_visits("
             "cluster_id INTEGER NOT NULL,"
             "visit_id INTEGER NOT NULL,"
             "score NUMERIC DEFAULT 0 NOT NULL,"
             "engagement_score NUMERIC DEFAULT 0 NOT NULL,"
             "url_for_deduping LONGVARCHAR NOT NULL,"
             "normalized_url LONGVARCHAR NOT NULL,"
             "url_for_display LONGVARCHAR NOT NULL,"
             "interaction_state INTEGER DEFAULT 0 NOT NULL,"
             "PRIMARY KEY(cluster_id,visit_id))"
             "WITHOUT ROWID") &&
         GetDB().Execute(
             "CREATE INDEX IF NOT EXISTS clusters_for_visit ON "
             "clusters_and_visits(visit_id)");
}

// Converts categories to something that can be stored in the database. As the
// serialized format is already being synced, the implementation of these
// functions should not be changed.
std::string VisitAnnotationsDatabase::ConvertCategoriesToStringColumn(
    const std::vector<VisitContentModelAnnotations::Category>& categories) {
  std::vector<std::string> serialized_categories;
  for (const auto& category : categories) {
    serialized_categories.emplace_back(category.ToString());
  }
  return base::JoinString(serialized_categories, ",");
}

// Converts serialized categories into a vector of (`id`, `weight`) pairs. As
// the serialized format is already being synced, the implementation of these
// functions should not be changed.
std::vector<VisitContentModelAnnotations::Category>
VisitAnnotationsDatabase::GetCategoriesFromStringColumn(
    const std::string& column_value) {
  std::vector<VisitContentModelAnnotations::Category> categories;

  std::vector<std::string> category_strings = base::SplitString(
      column_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& category_string : category_strings) {
    std::vector<std::string> category_parts = base::SplitString(
        category_string, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    auto category = VisitContentModelAnnotations::Category::FromStringVector(
        category_parts);
    if (category) {
      categories.emplace_back(*category);
    }
  }
  return categories;
}

// Serializes a vector of strings into a string that can be stored in the db.
// As the serialized format is already being synced, the implementation of
// these functions should not be changed.
std::string VisitAnnotationsDatabase::SerializeToStringColumn(
    const std::vector<std::string>& related_searches) {
  // Use the Null character as the separator to serialize the related searches.
  using std::string_literals::operator""s;
  return base::JoinString(related_searches, "\0"s);
}

// Converts a serialized db string into a vector of strings. As the serialized
// format is already being synced, the implementation of these functions
// should not be changed.
std::vector<std::string> VisitAnnotationsDatabase::DeserializeFromStringColumn(
    const std::string& column_value) {
  using std::string_literals::operator""s;
  return base::SplitString(column_value, "\0"s, base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

}  // namespace history
