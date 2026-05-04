// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/intent_table.h"

#include "base/notimplemented.h"
#include "components/accessibility_annotator/core/data_models/intent.h"
#include "components/history/core/browser/history_types.h"
#include "sql/database.h"
#include "sql/table_management_helpers.h"

namespace accessibility_annotator {

namespace {

constexpr char kTaskIntentProvenanceTableName[] = "task_intent_provenance";
constexpr char kIdColumn[] = "id";
constexpr char kTaskIntentIdColumn[] = "task_intent_id";
constexpr char kVisitIdColumn[] = "visit_id";
constexpr char kUrlIdColumn[] = "url_id";

constexpr char kTaskIntentTableName[] = "task_intent";
constexpr char kClusterMostRecentVisitTimeColumn[] =
    "cluster_most_recent_visit_time";
constexpr char kNeedsRegenerationColumn[] = "needs_regeneration";
constexpr char kTaskIntentTypeColumn[] = "task_intent_type";
constexpr char kTaskIntentColumn[] = "task_intent";
constexpr char kTaskIntentStatusTypeColumn[] = "task_intent_status_type";
constexpr char kTaskIntentStatusColumn[] = "task_intent_status";

}  // namespace

IntentTable::IntentTable() = default;
IntentTable::~IntentTable() = default;

bool IntentTable::Init(sql::Database* database) {
  if (!database) {
    return false;
  }

  database_ = database;
  return true;
}

bool IntentTable::AddOrUpdateTaskIntent(const TaskIntent& task_intent) {
  NOTIMPLEMENTED();
  return false;
}

std::vector<TaskIntent> IntentTable::GetTaskIntentsByStatusType(
    TaskIntentStatusType status_type) {
  NOTIMPLEMENTED();
  return {};
}

bool IntentTable::InvalidateTaskIntentsForDeletedHistory(
    const history::DeletionInfo& deletion_info) {
  NOTIMPLEMENTED();
  return false;
}

bool IntentTable::DeleteAllTaskIntents() {
  NOTIMPLEMENTED();
  return false;
}

bool IntentTable::MigrateFromCleanStateToVersion1() {
  if (!database_) {
    return false;
  }

  return sql::CreateTable(
             *database_, kTaskIntentProvenanceTableName,
             /*column_names_and_types=*/
             {{kIdColumn, "INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL"},
              {kTaskIntentIdColumn, "INTEGER NOT NULL"},
              {kVisitIdColumn, "INTEGER NOT NULL"},
              {kUrlIdColumn, "INTEGER NOT NULL"}}) &&
         sql::CreateTable(
             *database_, kTaskIntentTableName,
             /*column_names_and_types=*/
             {{kIdColumn, "INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL"},
              {kClusterMostRecentVisitTimeColumn, "INTEGER NOT NULL"},
              {kNeedsRegenerationColumn, "INTEGER NOT NULL"},
              {kTaskIntentTypeColumn, "INTEGER NOT NULL"},
              {kTaskIntentColumn, "TEXT"},
              {kTaskIntentStatusTypeColumn, "INTEGER NOT NULL"},
              {kTaskIntentStatusColumn, "TEXT"}});
}

}  // namespace accessibility_annotator
