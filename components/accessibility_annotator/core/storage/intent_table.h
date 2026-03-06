// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_INTENT_TABLE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_INTENT_TABLE_H_

#include <vector>

#include "base/memory/raw_ptr.h"

namespace history {
class DeletionInfo;
}  // namespace history

namespace sql {
class Database;
}  // namespace sql

namespace accessibility_annotator {

struct TaskIntent;
enum class TaskIntentStatusType;

// This class manages the tables storing task intents and provenance metadata
// for clusters of history within the SQLite database passed to initialization.
// It expects the following schemas:
//
// -----------------------------------------------------------------------------
// history_cluster_provenance   Contains provenance metadata for clusters of
//                              history entries.
//
//   visit_id                           Uniquely identifies a visit to a URL.
//   url_id                             Uniquely identifies a URL.
//   cluster_id                         Uniquely identifies a cluster of history
//                                      entries.
// -----------------------------------------------------------------------------
// task_intent                  Contains task-level intents.
//
//   cluster_id                         Uniquely identifies a cluster of history
//                                      entries (primary key, as well as foreign
//                                      key to the history_cluster_provenance
//                                      table).
//   cluster_most_recent_visit_time     The most recent visit time for the
//                                      cluster of history entries, in
//                                      microseconds since the epoch.
//   task_intent_type                   Enumerated type of task intent.
//   task_intent                        A string representation of the task
//                                      intent.
//   task_intent_status_type            Enumerated type of task intent status.
//   task_intent_status                 A string representation of the task
//                                      intent status.
// -----------------------------------------------------------------------------
class IntentTable {
 public:
  IntentTable();
  IntentTable(const IntentTable&) = delete;
  IntentTable& operator=(const IntentTable&) = delete;
  ~IntentTable();

  // Initializes the table with the given SQLite database. Returns true on
  // success. Must be called before any other methods.
  bool Init(sql::Database* database);

  // Adds a new task intent to the table, or updates an existing task intent if
  // one already exists with the same cluster ID. Returns true on success.
  bool AddOrUpdateTaskIntent(const TaskIntent& task_intent);

  // Returns all task intents matching the given `TaskIntentStatusType`.
  std::vector<TaskIntent> GetTaskIntentsByStatusType(
      TaskIntentStatusType status_type);

  // Invalidates all task intents sourced from history deleted in the given
  // `DeletionInfo`. Returns true on success.
  bool InvalidateTaskIntentsForDeletedHistory(
      const history::DeletionInfo& deletion_info);

  // Deletes all task intents from the tableUsed when all history is deleted.
  // Returns true on success.
  bool DeleteAllTaskIntents();

 private:
  raw_ptr<sql::Database> database_ = nullptr;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_INTENT_TABLE_H_
