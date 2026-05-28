// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_DATABASE_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_DATABASE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "sql/database.h"

namespace record_replay {

// Manages the relational SQLite database for record/replay recordings,
// task definitions, and execution observations.
class TaskDatabase {
 public:
  TaskDatabase();
  TaskDatabase(const TaskDatabase&) = delete;
  TaskDatabase& operator=(const TaskDatabase&) = delete;
  ~TaskDatabase();

  // Initializes the database in the given profile directory. If `profile_path`
  // is empty, the database is opened in memory which suffices for testing.
  void Init(const base::FilePath& profile_path);

  // Adds a recording to the database. Returns the generated ID of the newly
  // added recording, or -1 on failure.
  int64_t AddRecording(Recording recording);

  // Retrieves every Recording that matches the given `url`.
  std::vector<Recording> GetRecordingsByUrl(std::string_view url);

  // Attempts to seed from file (if path not empty), then Finch (if json not
  // empty). Triggers DumpWithoutCrashing if all attempted seeding mechanisms
  // fail.
  void RunSeeding(const base::FilePath& file_path,
                  std::string_view feature_json);

  // Full CRUD for Task Definitions (Create & Update are unified under Save)
  int64_t SaveTaskDefinition(std::optional<int64_t> definition_id,
                             TaskDefinition definition);
  std::optional<TaskDefinition> GetTaskDefinition(int64_t definition_id);
  std::vector<TaskDefinition> GetTaskDefinitionsByUrl(std::string_view url);
  bool DeleteTaskDefinition(int64_t definition_id);

  // Full CRUD for Task Observations (Create & Update are unified under Save)
  int64_t SaveObservation(TaskObservation observation);
  std::vector<TaskObservation> GetObservationsForDefinition(
      int64_t definition_id);
  bool DeleteObservation(int64_t observation_id);

 private:
  // Returns the current version of the database.
  int GetDatabaseVersion();

  // Migrates the database to the current version.
  bool Migrate(int version);

  // Creates the "recordings" table if it doesn't exist.
  bool CreateRecordingsTable();

  // Creates the "task_definitions" table if it doesn't exist.
  bool CreateTaskDefinitionsTable();

  // Creates the "task_steps" table if it doesn't exist.
  bool CreateTaskStepsTable();

  // Creates the "task_parameters" table if it doesn't exist.
  bool CreateTaskParametersTable();

  // Creates the "task_observations" table if it doesn't exist.
  bool CreateTaskObservationsTable();

  // Creates the "task_parameter_values" table if it doesn't exist.
  bool CreateTaskParameterValuesTable();

  // Prunes old observations (>365 days).
  bool PruneOldObservations(base::TimeDelta max_age);

  // Reads a file and seeds the database if empty.
  base::expected<std::vector<TaskDefinition>, std::string>
  SeedTaskDefinitionsFromFile(const base::FilePath& file_path);

  // Checks if the "task_definitions" table is empty.
  bool IsTaskDefinitionsTableEmpty();

  // Reads JSON and seeds the database if empty.
  base::expected<std::vector<TaskDefinition>, std::string>
  GetSeedTaskDefinitionsFromJson(const std::string& json_string);

  // Saves a batch of seeded definitions within an atomic transaction.
  void SaveSeededTaskDefinitions(std::vector<TaskDefinition> definitions);

  // Task Definition helpers:
  bool HasDefinitionWithId(std::optional<int64_t> definition_id);
  bool AddShallowTaskDefinition(const TaskDefinition& definition);
  bool UpdateShallowTaskDefinition(int64_t definition_id,
                                   const TaskDefinition& definition);

  // Step Definition helpers:
  bool SaveSteps(int64_t definition_id,
                 ::google::protobuf::RepeatedPtrField<TaskStep> steps);
  base::flat_map<int64_t, int32_t> GetStepIndicesAndIds(int64_t definition_id);
  bool ShiftStepIndicesNegative(
      const base::flat_map<int64_t, int32_t>& step_index_for_id);
  bool UpdateStep(int64_t step_id, const TaskStep& step);
  bool AddStep(int64_t definition_id, const TaskStep& step);
  std::optional<int64_t> FindStepIndex(
      const TaskStep& step,
      const base::flat_map<int64_t, int32_t>& step_index_for_id);
  bool DeleteStepById(int64_t step_id);

  // Task Parameter helpers:
  bool SaveParameters(
      int64_t step_id,
      ::google::protobuf::RepeatedPtrField<TaskParameter> parameters);
  base::flat_map<int64_t, std::string> GetParameterIndicesAndKeys(
      int64_t step_id);
  bool UpdateParameter(const TaskParameter& parameter, int64_t parameter_id);
  bool AddParameter(int64_t step_id, const TaskParameter& parameter);
  std::optional<int64_t> FindParameterId(
      const TaskParameter& parameter,
      const base::flat_map<int64_t, std::string>& parameter_key_for_id);
  bool DeleteParameterById(int64_t parameter_id);

  // Task Retrieval helpers:
  std::optional<TaskDefinition> GetPartialDefinition(int64_t definition_id);
  base::flat_map<int64_t, std::vector<TaskParameter>>
  GetParametersForStepsOfDefinition(int64_t definition_id);
  std::vector<TaskStep> GetStepsOfDefinition(
      int64_t definition_id,
      base::flat_map<int64_t, std::vector<TaskParameter>> step_params);

  // Task Observation helpers:
  bool HasObservationWithId(int64_t observation_id);
  std::optional<int64_t> AddObservation(const TaskObservation& observation);
  bool UpdateObservation(int64_t observation_id,
                         const TaskObservation& observation);
  base::flat_map<std::pair<int32_t, std::string>, int64_t>
  GetParameterIdsForDefinition(int64_t definition_id);
  bool HasValueForParameter(int64_t parameter_id, int64_t observation_id);
  bool AddParameterValue(int64_t parameter_id,
                         int64_t observation_id,
                         const std::string& value);
  bool UpdateParameterValue(int64_t parameter_id,
                            int64_t observation_id,
                            const std::string& value);
  std::optional<std::string> GetParameterValue(int64_t parameter_id,
                                               int64_t observation_id);

  sql::Database db_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_DATABASE_H_
