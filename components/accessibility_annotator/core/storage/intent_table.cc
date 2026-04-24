// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/intent_table.h"

#include "base/notimplemented.h"
#include "components/accessibility_annotator/core/data_models/intent.h"
#include "components/history/core/browser/history_types.h"
#include "sql/database.h"

namespace {

// TABLE CREATION STATEMENTS
// -----------------------------------------------------------------------------
// Table creation should be pegged to a specific version number, enforcing
// linear migration-only updates.

// TODO(crbug.com/491051120): Update table creation statements with common SQL
// utilities.

constexpr char kTaskIntentProvenanceTableVersion1CreationSql[] = R"SQL(
  CREATE TABLE task_intent_provenance (
    id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    task_intent_id INTEGER NOT NULL,
    visit_id INTEGER NOT NULL,
    url_id INTEGER NOT NULL
  )
)SQL";

constexpr char kTaskIntentTableVersion1CreationSql[] = R"SQL(
  CREATE TABLE task_intent (
    id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    cluster_most_recent_visit_time INTEGER NOT NULL,
    needs_regeneration INTEGER NOT NULL,
    task_intent_type INTEGER NOT NULL,
    task_intent TEXT,
    task_intent_status_type INTEGER NOT NULL,
    task_intent_status TEXT
  )
)SQL";

}  // namespace

namespace accessibility_annotator {

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

  if (!database_->Execute(kTaskIntentProvenanceTableVersion1CreationSql)) {
    return false;
  }

  if (!database_->Execute(kTaskIntentTableVersion1CreationSql)) {
    return false;
  }

  return true;
}

}  // namespace accessibility_annotator
