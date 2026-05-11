// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_DATABASE_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_DATABASE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "sql/database.h"

namespace record_replay {

// Manages the relational SQLite database for record/replay recordings,
// task definitions, and task data.
//
// SQLite allows structured queries, such as retrieving all task definitions for
// a given site or managing data overrides independently of shareable intent.
class TaskDatabase {
 public:
  TaskDatabase();
  TaskDatabase(const TaskDatabase&) = delete;
  TaskDatabase& operator=(const TaskDatabase&) = delete;
  ~TaskDatabase();

  // Initializes the database in the given profile directory. If `profile_path`
  // is empty, the database is opened in memory which suffices for testing.
  void Init(base::FilePath profile_path);

  // Adds a recording to the database. Returns the generated ID of the newly
  // added recording, or -1 on failure.
  int64_t AddRecording(Recording recording);

  // Retrieves every Recording that matches the given `url`.
  std::vector<Recording> GetRecordingsByUrl(std::string url);

  // Handles both insertion (when task_definition_id is nullopt) and updates.
  void SaveTaskDefinition(std::optional<int64_t> task_definition_id,
                          TaskDefinition task_definition,
                          std::string target_url,
                          std::optional<int64_t> recording_id);

  // Attempts to seed from file (if path not empty), then Finch (if json not
  // empty). Triggers DumpWithoutCrashing if all attempted seeding mechanisms
  // fail.
  void RunSeeding(base::FilePath file_path, std::string feature_json);

  // Retrieves the task definition for a given ID, if it exists.
  std::optional<TaskDefinition> GetTaskDefinition(int64_t task_definition_id);

  // Retrieves all task definitions for a site, returning their IDs and proto
  // data.
  std::vector<std::pair<int64_t, TaskDefinition>> GetTaskDefinitionsByUrl(
      const std::string& url);

  // Saves or updates task data for a task definition.
  bool SaveTaskData(int64_t task_definition_id, const TaskData& data);

  // Retrieves task data for a task definition.
  std::optional<TaskData> GetTaskData(int64_t task_definition_id);

  // Deletes task data for a task definition.
  bool DeleteTaskData(int64_t task_definition_id);

  // Deletes a task definition.
  bool DeleteTaskDefinition(int64_t task_definition_id);

 private:
  // Returns the current version of the database.
  int GetDatabaseVersion();

  // Migrates the database to the current version.
  bool Migrate(int version);

  // Creates the "Recordings" table if it doesn't exist.
  bool CreateRecordingsTable();

  // Creates the "TaskDefinitions" table if it doesn't exist.
  bool CreateTaskDefinitionsTable();

  // Creates the "TaskData" table if it doesn't exist.
  bool CreateTaskDataTable();

  // Reads a file and seeds the database if empty.
  base::expected<std::vector<TaskDefinition>, std::string>
  SeedTaskDefinitionsFromFile(const base::FilePath& file_path);

  // Checks if the "TaskDefinitions" table is empty.
  bool IsTaskDefinitionsTableEmpty();

  // Reads JSON and seeds the database if empty.
  base::expected<std::vector<TaskDefinition>, std::string>
  GetSeedTaskDefinitionsFromJson(const std::string& json_string);

  // Saves a batch of seeded task definitions within an atomic transaction.
  void SaveSeededTaskDefinitions(std::vector<TaskDefinition> task_definitions);

  sql::Database db_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_DATABASE_H_
