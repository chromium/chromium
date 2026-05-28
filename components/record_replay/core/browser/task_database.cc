// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_database.h"

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/record_replay/core/browser/parsing_utils.h"
#include "components/record_replay/core/browser/task_definition_parsing_utils.h"
#include "components/record_replay/core/common/record_replay_switches.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace record_replay {

namespace {
// Current version of the database schema.
constexpr int kVersionNumber = 1;

constexpr base::FilePath::StringViewType kTaskDatabaseFileName =
    FILE_PATH_LITERAL("ReplayTaskDatabase.db");

std::string NormalizeUrl(const std::string& url_string) {
  GURL url(url_string);
  if (!url.is_valid()) {
    return url_string;
  }
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements).spec();
}
}  // namespace

TaskDatabase::TaskDatabase() : db_(sql::Database::Tag("ReplayTasks")) {}
TaskDatabase::~TaskDatabase() = default;

void TaskDatabase::Init(const base::FilePath& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.is_open()) {
    return;
  }

  db_.set_error_callback(
      base::BindRepeating([](int error, sql::Statement* statement) {
        DLOG(ERROR) << "TaskDatabase SQLite Error: " << error
                    << (statement ? base::StrCat({", SQL: ",
                                                  statement->GetSQLStatement()})
                                  : "");
      }));

  // Open the database file. If this fails, abort immediately.
  if (!db_.Open(profile_path.Append(kTaskDatabaseFileName))) {
    DLOG(ERROR) << "Failed to open TaskDatabase at: " << profile_path.value();
    return;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    DLOG(ERROR) << "Failed to begin transaction for schema creation.";
    db_.Close();
    return;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWipeRecordings)) {
    std::ignore = db_.Execute("DROP TABLE IF EXISTS task_parameters");
    std::ignore = db_.Execute("DROP TABLE IF EXISTS task_steps");
    std::ignore = db_.Execute("DROP TABLE IF EXISTS task_definitions");
    std::ignore = db_.Execute("DROP TABLE IF EXISTS recordings");
    std::ignore = db_.Execute("DROP TABLE IF EXISTS task_parameter_values");
    std::ignore = db_.Execute("DROP TABLE IF EXISTS task_observations");
  }

  if (!CreateRecordingsTable()) {
    DLOG(ERROR) << "Failed to create recordings table.";
    db_.Close();
    return;
  }

  if (!CreateTaskDefinitionsTable()) {
    DLOG(ERROR) << "Failed to create task_definitions table.";
    db_.Close();
    return;
  }

  if (!CreateTaskStepsTable()) {
    DLOG(ERROR) << "Failed to create task_steps table.";
    db_.Close();
    return;
  }

  if (!CreateTaskParametersTable()) {
    DLOG(ERROR) << "Failed to create task_parameters table.";
    db_.Close();
    return;
  }

  if (!CreateTaskObservationsTable()) {
    DLOG(ERROR) << "Failed to create task_observations table.";
    db_.Close();
    return;
  }

  if (!CreateTaskParameterValuesTable()) {
    DLOG(ERROR) << "Failed to create task_parameter_values table.";
    db_.Close();
    return;
  }

  if (!Migrate(GetDatabaseVersion())) {
    DLOG(ERROR) << "Failed to migrate database.";
    db_.Close();
    return;
  }

  transaction.Commit();

  std::ignore = PruneOldObservations(base::Days(365));
}

int TaskDatabase::GetDatabaseVersion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db_.GetUniqueStatement("PRAGMA user_version"));
  return statement.Step() ? statement.ColumnInt(0) : 0;
}

bool TaskDatabase::Migrate(int version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return db_.Execute(base::StrCat(
      {"PRAGMA user_version = ", base::NumberToString(kVersionNumber)}));
}

bool TaskDatabase::CreateRecordingsTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.DoesTableExist("recordings")) {
    return true;
  }

  static constexpr char kSql[] =
      // clang-format off
      "CREATE TABLE recordings("
          "id INTEGER PRIMARY KEY NOT NULL,"
          "url TEXT NOT NULL,"
          "start_time INTEGER NOT NULL,"
          "name TEXT NOT NULL,"
          "proto BLOB NOT NULL)";
  // clang-format on
  if (!db_.Execute(kSql)) {
    return false;
  }

  // Optimize GetRecordingsByUrl queries, which filter by URL and retrieve in
  // descending chronological order. Avoids SQLite in-memory filesorts.
  return db_.Execute(
      "CREATE INDEX IF NOT EXISTS recordings_url_timestamp "
      "ON recordings(url,start_time DESC)");
}

bool TaskDatabase::CreateTaskDefinitionsTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.DoesTableExist("task_definitions")) {
    return true;
  }

  static constexpr char kSql[] =
      // clang-format off
      "CREATE TABLE task_definitions("
          "definition_id INTEGER PRIMARY KEY NOT NULL,"
          "recording_id INTEGER,"
          "title TEXT NOT NULL,"
          "url TEXT NOT NULL,"
          "description TEXT NOT NULL)";
  // clang-format on
  if (!db_.Execute(kSql)) {
    return false;
  }

  return db_.Execute(
      "CREATE INDEX IF NOT EXISTS idx_task_definitions_url "
      "ON task_definitions(url)");
}

bool TaskDatabase::CreateTaskStepsTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.DoesTableExist("task_steps")) {
    return true;
  }

  static constexpr char kSql[] =
      // clang-format off
      "CREATE TABLE task_steps("
          "step_id INTEGER PRIMARY KEY NOT NULL,"
          "definition_id INTEGER NOT NULL,"
          "step_index INTEGER NOT NULL,"
          "url TEXT NOT NULL,"
          "description TEXT NOT NULL)";
  // clang-format on
  if (!db_.Execute(kSql)) {
    return false;
  }

  if (!db_.Execute("CREATE UNIQUE INDEX IF NOT EXISTS idx_task_steps_def_index "
                   "ON task_steps(definition_id,step_index)")) {
    return false;
  }

  if (!db_.Execute(
          "CREATE INDEX IF NOT EXISTS idx_task_steps_url ON task_steps(url)")) {
    return false;
  }
  return db_.Execute(
      "CREATE INDEX IF NOT EXISTS idx_task_steps_definition ON "
      "task_steps(definition_id)");
}

bool TaskDatabase::CreateTaskParametersTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.DoesTableExist("task_parameters")) {
    return true;
  }

  static constexpr char kSql[] =
      // clang-format off
      "CREATE TABLE task_parameters("
          "parameter_id INTEGER PRIMARY KEY NOT NULL,"
          "step_id INTEGER NOT NULL,"
          "parameter_key TEXT NOT NULL,"
          "name TEXT NOT NULL,"
          "type TEXT NOT NULL,"
          "description TEXT NOT NULL,"
          "extraction_strategy BLOB)";
  // clang-format on
  if (!db_.Execute(kSql)) {
    return false;
  }

  if (!db_.Execute(
          "CREATE UNIQUE INDEX IF NOT EXISTS idx_task_parameters_step_key "
          "ON task_parameters(step_id,parameter_key)")) {
    return false;
  }

  return db_.Execute(
      "CREATE INDEX IF NOT EXISTS idx_task_parameters_step ON "
      "task_parameters(step_id)");
}

bool TaskDatabase::CreateTaskObservationsTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.DoesTableExist("task_observations")) {
    return true;
  }

  static constexpr char kSql[] =
      "CREATE TABLE task_observations("
      "observation_id INTEGER PRIMARY KEY NOT NULL,"
      "definition_id INTEGER NOT NULL,"
      "start_time INTEGER NOT NULL,"
      "end_time INTEGER NOT NULL,"
      "execution_source INTEGER NOT NULL)";
  if (!db_.Execute(kSql)) {
    return false;
  }

  if (!db_.Execute("CREATE INDEX IF NOT EXISTS idx_task_observations_def_time "
                   "ON task_observations(definition_id, start_time DESC)")) {
    return false;
  }
  return db_.Execute(
      "CREATE INDEX IF NOT EXISTS idx_task_observations_start_time "
      "ON task_observations(start_time)");
}

bool TaskDatabase::CreateTaskParameterValuesTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.DoesTableExist("task_parameter_values")) {
    return true;
  }

  static constexpr char kSql[] =
      "CREATE TABLE task_parameter_values("
      "value_id INTEGER PRIMARY KEY NOT NULL,"
      "parameter_id INTEGER NOT NULL,"
      "observation_id INTEGER NOT NULL,"
      "value TEXT NOT NULL)";
  if (!db_.Execute(kSql)) {
    return false;
  }

  if (!db_.Execute("CREATE UNIQUE INDEX IF NOT EXISTS "
                   "idx_task_parameter_values_param_obs "
                   "ON task_parameter_values(parameter_id, observation_id)")) {
    return false;
  }

  if (!db_.Execute(
          "CREATE INDEX IF NOT EXISTS idx_task_parameter_values_observation "
          "ON task_parameter_values(observation_id)")) {
    return false;
  }
  return db_.Execute(
      "CREATE INDEX IF NOT EXISTS idx_task_parameter_values_parameter "
      "ON task_parameter_values(parameter_id)");
}

bool TaskDatabase::IsTaskDefinitionsTableEmpty() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      // clang-format off
      "SELECT EXISTS(SELECT 1 FROM task_definitions)";
  // clang-format on
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  return statement.Step() && statement.ColumnInt(0) == 0;
}

base::expected<std::vector<TaskDefinition>, std::string>
TaskDatabase::GetSeedTaskDefinitionsFromJson(const std::string& json_string) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsTaskDefinitionsTableEmpty()) {
    return std::vector<TaskDefinition>();
  }

  std::vector<base::Value> values = ParseJSONListOfDicts(json_string);
  std::vector<TaskDefinition> seeded_task_definitions;
  int index = 0;
  for (const base::Value& item : values) {
    if (!item.is_dict()) {
      return base::unexpected(
          base::StrCat({"Task definition item at index ",
                        base::NumberToString(index), " is not a dictionary."}));
    }

    base::expected<TaskDefinition, std::string> result =
        ParseTaskDefinition(item.GetDict());
    if (!result.has_value()) {
      return base::unexpected(
          base::StrCat({"Error in item ", base::NumberToString(index), ": ",
                        result.error()}));
    }

    seeded_task_definitions.push_back(std::move(result.value()));
    index++;
  }
  return seeded_task_definitions;
}

base::expected<std::vector<TaskDefinition>, std::string>
TaskDatabase::SeedTaskDefinitionsFromFile(const base::FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsTaskDefinitionsTableEmpty()) {
    return std::vector<TaskDefinition>();
  }

  std::string json_string;
  if (!base::ReadFileToString(file_path, &json_string)) {
    return base::unexpected(base::StrCat(
        {"Failed to read task definitions file: ", file_path.AsUTF8Unsafe()}));
  }

  return GetSeedTaskDefinitionsFromJson(json_string);
}

void TaskDatabase::RunSeeding(const base::FilePath& file_path,
                              std::string_view feature_json) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Skip if already seeded.
  if (!IsTaskDefinitionsTableEmpty()) {
    return;
  }

  std::string first_error;

  // Try seeding from a local file first to give developer overrides priority.
  if (!file_path.empty()) {
    if (base::expected<std::vector<TaskDefinition>, std::string> result =
            SeedTaskDefinitionsFromFile(file_path);
        result.has_value()) {
      SaveSeededTaskDefinitions(std::move(result.value()));
      return;
    } else {
      first_error = std::move(result.error());
    }
  }

  // Fall back to Finch seeding if the local file was not specified or failed.
  if (!feature_json.empty()) {
    if (base::expected<std::vector<TaskDefinition>, std::string> result =
            GetSeedTaskDefinitionsFromJson(std::string(feature_json));
        result.has_value()) {
      SaveSeededTaskDefinitions(std::move(result.value()));
      return;
    } else if (first_error.empty()) {
      first_error = std::move(result.error());
    }
  }

  if (!first_error.empty()) {
    DLOG(ERROR) << first_error;
  }
}

void TaskDatabase::SaveSeededTaskDefinitions(
    std::vector<TaskDefinition> definitions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Transaction transaction(&db_);
  if (transaction.Begin()) {
    for (TaskDefinition& definition : definitions) {
      std::string definition_title = definition.title();
      if (SaveTaskDefinition(std::nullopt, std::move(definition)) == 0) {
        DLOG(ERROR) << "Seeding failed for definition: " << definition_title;
        return;  // Rollback on transaction destruction
      }
    }
    std::ignore = transaction.Commit();
  }
}

int64_t TaskDatabase::AddRecording(Recording recording) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  recording.clear_id();
  std::string serialized_proto = recording.SerializeAsString();

  static constexpr char kSql[] =
      // clang-format off
      "INSERT INTO recordings(url,start_time,name,proto) "
      "VALUES(?,?,?,?)";
  // clang-format on
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));

  statement.BindString(0, std::move(*recording.mutable_url()));
  statement.BindInt64(1, recording.start_time());
  statement.BindString(2, std::move(*recording.mutable_name()));
  statement.BindBlob(3, std::move(serialized_proto));

  return statement.Run() ? db_.GetLastInsertRowId() : -1;
}

std::vector<Recording> TaskDatabase::GetRecordingsByUrl(std::string_view url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      // clang-format off
      "SELECT id,proto FROM recordings WHERE url=? ORDER BY start_time DESC";
  // clang-format on
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, std::string(url));

  std::vector<Recording> recordings;
  while (statement.Step()) {
    int64_t id = statement.ColumnInt64(0);
    Recording recording;
    if (recording.ParseFromString(statement.ColumnBlobAsString(1))) {
      recording.set_id(id);
      recordings.push_back(std::move(recording));
    }
  }

  return recordings;
}

int64_t TaskDatabase::SaveTaskDefinition(std::optional<int64_t> definition_id,
                                         TaskDefinition definition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return 0;
  }

  if (!definition_id && definition.has_id()) {
    definition_id = definition.id();
  }

  if (HasDefinitionWithId(definition_id)) {
    if (!UpdateShallowTaskDefinition(*definition_id, definition)) {
      return 0;
    }
  } else {
    if (!AddShallowTaskDefinition(definition)) {
      return 0;
    }
    definition_id = db_.GetLastInsertRowId();
  }

  if (!SaveSteps(*definition_id, std::move(*definition.mutable_task_steps()))) {
    return 0;
  }

  return transaction.Commit() ? *definition_id : 0;
}

std::optional<TaskDefinition> TaskDatabase::GetPartialDefinition(
    int64_t definition_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      // clang-format off
      "SELECT recording_id,title,url,description FROM task_definitions "
      "WHERE definition_id=?";
  // clang-format on
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, definition_id);

  if (!statement.Step()) {
    return std::nullopt;
  }

  TaskDefinition definition;
  definition.set_id(definition_id);
  if (statement.GetColumnType(0) != sql::ColumnType::kNull) {
    definition.set_recording_id(statement.ColumnInt64(0));
  }
  definition.set_title(statement.ColumnString(1));
  definition.set_url(statement.ColumnString(2));
  definition.set_description(statement.ColumnString(3));
  return definition;
}

base::flat_map<int64_t, std::vector<TaskParameter>>
TaskDatabase::GetParametersForStepsOfDefinition(int64_t definition_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      // clang-format off
      "SELECT parameter_id,step_id,parameter_key,name,type,description,"
      "extraction_strategy FROM task_parameters "
      "WHERE step_id IN (SELECT step_id FROM task_steps WHERE definition_id=?) "
      "ORDER BY parameter_id ASC";
  // clang-format on
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, definition_id);

  base::flat_map<int64_t, std::vector<TaskParameter>> step_params;
  while (statement.Step()) {
    int64_t step_id = statement.ColumnInt64(1);
    TaskParameter param;
    param.set_id(statement.ColumnInt64(0));
    param.set_key(statement.ColumnString(2));
    param.set_name(statement.ColumnString(3));
    param.set_type(statement.ColumnString(4));
    param.set_description(statement.ColumnString(5));
    if (statement.GetColumnType(6) != sql::ColumnType::kNull) {
      param.mutable_extraction_strategy()->ParseFromString(
          statement.ColumnBlobAsString(6));
    }
    step_params[step_id].push_back(std::move(param));
  }
  return step_params;
}

std::vector<TaskStep> TaskDatabase::GetStepsOfDefinition(
    int64_t definition_id,
    base::flat_map<int64_t, std::vector<TaskParameter>> step_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      // clang-format off
      "SELECT step_id,step_index,url,description FROM task_steps "
      "WHERE definition_id=? ORDER BY step_index ASC";
  // clang-format on
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, definition_id);

  std::vector<TaskStep> steps;
  while (statement.Step()) {
    TaskStep step;
    int64_t step_id = statement.ColumnInt64(0);
    step.set_id(step_id);
    step.set_step_index(statement.ColumnInt(1));
    step.set_url(statement.ColumnString(2));
    step.set_description(statement.ColumnString(3));

    auto it = step_params.find(step_id);
    if (it != step_params.end()) {
      for (TaskParameter& param : it->second) {
        *step.add_parameters() = std::move(param);
      }
    }
    steps.push_back(std::move(step));
  }
  return steps;
}

std::optional<TaskDefinition> TaskDatabase::GetTaskDefinition(
    int64_t definition_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<TaskDefinition> definition =
      GetPartialDefinition(definition_id);
  if (!definition) {
    return std::nullopt;
  }

  base::flat_map<int64_t, std::vector<TaskParameter>> step_params =
      GetParametersForStepsOfDefinition(definition_id);

  std::vector<TaskStep> steps =
      GetStepsOfDefinition(definition_id, std::move(step_params));

  for (TaskStep& step : steps) {
    *definition->add_task_steps() = std::move(step);
  }

  return definition;
}

std::vector<TaskDefinition> TaskDatabase::GetTaskDefinitionsByUrl(
    std::string_view url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      // clang-format off
      "SELECT definition_id FROM task_definitions WHERE url=? "
      "ORDER BY definition_id ASC";
  // clang-format on
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, NormalizeUrl(std::string(url)));

  std::vector<TaskDefinition> definitions;
  while (statement.Step()) {
    int64_t definition_id = statement.ColumnInt64(0);
    if (std::optional<TaskDefinition> definition =
            GetTaskDefinition(definition_id)) {
      definitions.push_back(std::move(*definition));
    }
  }
  return definitions;
}

bool TaskDatabase::DeleteTaskDefinition(int64_t definition_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  // 1. Delete associated parameter values of observations of this definition.
  static constexpr char kDeleteValuesSql[] =
      "DELETE FROM task_parameter_values WHERE observation_id IN ("
      "SELECT observation_id FROM task_observations WHERE definition_id=?)";
  sql::Statement delete_values(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteValuesSql));
  delete_values.BindInt64(0, definition_id);
  if (!delete_values.Run()) {
    return false;
  }

  // 2. Delete task observations associated with this definition.
  static constexpr char kDeleteObsSql[] =
      "DELETE FROM task_observations WHERE definition_id=?";
  sql::Statement delete_obs(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteObsSql));
  delete_obs.BindInt64(0, definition_id);
  if (!delete_obs.Run()) {
    return false;
  }

  // 3. Delete orphaned parameter values.
  static constexpr char kDeleteParamsSql[] =
      "DELETE FROM task_parameters WHERE step_id IN "
      "(SELECT step_id FROM task_steps WHERE definition_id=?)";
  sql::Statement delete_params(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteParamsSql));
  delete_params.BindInt64(0, definition_id);
  if (!delete_params.Run()) {
    return false;
  }

  // 4. Delete steps associated with this definition.
  static constexpr char kDeleteStepsSql[] =
      "DELETE FROM task_steps WHERE definition_id=?";
  sql::Statement delete_steps(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteStepsSql));
  delete_steps.BindInt64(0, definition_id);
  if (!delete_steps.Run()) {
    return false;
  }

  // 5. Delete definition itself.
  static constexpr char kDeleteDefSql[] =
      "DELETE FROM task_definitions WHERE definition_id=?";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteDefSql));
  statement.BindInt64(0, definition_id);
  if (!statement.Run()) {
    return false;
  }

  return transaction.Commit();
}

int64_t TaskDatabase::SaveObservation(TaskObservation observation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!observation.definition().has_id()) {
    DLOG(ERROR) << "SaveObservation failed: definition id missing.";
    return 0;
  }
  if (!HasDefinitionWithId(observation.definition().id())) {
    DLOG(ERROR) << "SaveObservation failed: definition id "
                << observation.definition().id() << " does not exist.";
    return 0;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return 0;
  }

  bool exists = observation.has_id() && HasObservationWithId(observation.id());
  int64_t final_observation_id = 0;

  if (exists) {
    final_observation_id = observation.id();
    if (!UpdateObservation(final_observation_id, observation)) {
      return 0;
    }
  } else {
    std::optional<int64_t> added_id = AddObservation(observation);
    if (!added_id) {
      return 0;
    }
    final_observation_id = *added_id;
  }

  base::flat_map<std::pair<int32_t, std::string>, int64_t> parameter_id_map =
      GetParameterIdsForDefinition(observation.definition().id());

  for (const TaskStep& step : observation.definition().task_steps()) {
    int32_t step_index = step.step_index();
    for (const TaskParameter& param : step.parameters()) {
      auto it = parameter_id_map.find(std::make_pair(step_index, param.key()));
      if (it == parameter_id_map.end()) {
        continue;
      }
      int64_t parameter_id = it->second;

      if (HasValueForParameter(parameter_id, final_observation_id)) {
        if (!UpdateParameterValue(parameter_id, final_observation_id,
                                  param.value())) {
          return 0;
        }
      } else {
        if (!AddParameterValue(parameter_id, final_observation_id,
                               param.value())) {
          return 0;
        }
      }
    }
  }

  if (!transaction.Commit()) {
    return 0;
  }
  return final_observation_id;
}

std::vector<TaskObservation> TaskDatabase::GetObservationsForDefinition(
    int64_t definition_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kSql[] =
      "SELECT observation_id, start_time, end_time, execution_source FROM "
      "task_observations WHERE definition_id=? ORDER BY start_time DESC";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, definition_id);

  std::vector<TaskObservation> observations;
  while (statement.Step()) {
    TaskObservation observation;
    int64_t observation_id = statement.ColumnInt64(0);
    observation.set_id(observation_id);
    observation.set_start_time(statement.ColumnInt64(1));
    observation.set_end_time(statement.ColumnInt64(2));
    observation.set_execution_source(
        static_cast<ExecutionSource>(statement.ColumnInt(3)));

    if (std::optional<TaskDefinition> definition =
            GetTaskDefinition(definition_id)) {
      for (TaskStep& step : *definition->mutable_task_steps()) {
        for (TaskParameter& param : *step.mutable_parameters()) {
          if (std::optional<std::string> val =
                  GetParameterValue(param.id(), observation_id)) {
            param.set_value(std::move(*val));
          }
        }
      }
      *observation.mutable_definition() = std::move(*definition);
    }

    observations.push_back(std::move(observation));
  }

  return observations;
}

bool TaskDatabase::DeleteObservation(int64_t observation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  // 1. Delete associated parameter values.
  static constexpr char kDeleteValuesSql[] =
      "DELETE FROM task_parameter_values WHERE observation_id=?";
  sql::Statement delete_values(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteValuesSql));
  delete_values.BindInt64(0, observation_id);
  if (!delete_values.Run()) {
    return false;
  }

  // 2. Delete the observation itself.
  static constexpr char kDeleteObsSql[] =
      "DELETE FROM task_observations WHERE observation_id=?";
  sql::Statement delete_obs(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteObsSql));
  delete_obs.BindInt64(0, observation_id);
  if (!delete_obs.Run()) {
    return false;
  }

  return transaction.Commit();
}

bool TaskDatabase::PruneOldObservations(base::TimeDelta max_age) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  int64_t threshold =
      (base::Time::Now() - max_age).ToDeltaSinceWindowsEpoch().InMicroseconds();

  // 1. Delete associated parameter values of pruned observations.
  static constexpr char kDeleteValuesSql[] =
      "DELETE FROM task_parameter_values WHERE observation_id IN ("
      "SELECT observation_id FROM task_observations WHERE start_time < ?)";
  sql::Statement delete_values(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteValuesSql));
  delete_values.BindInt64(0, threshold);
  if (!delete_values.Run()) {
    return false;
  }

  // 2. Delete observations themselves.
  static constexpr char kDeleteObsSql[] =
      "DELETE FROM task_observations WHERE start_time < ?";
  sql::Statement delete_obs(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteObsSql));
  delete_obs.BindInt64(0, threshold);
  if (!delete_obs.Run()) {
    return false;
  }

  return transaction.Commit();
}

// Task Observation helpers:
bool TaskDatabase::HasObservationWithId(int64_t observation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "SELECT 1 FROM task_observations WHERE observation_id=?";
  sql::Statement stmt(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  stmt.BindInt64(0, observation_id);
  return stmt.Step();
}

std::optional<int64_t> TaskDatabase::AddObservation(
    const TaskObservation& observation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "INSERT INTO task_observations(observation_id, definition_id, "
      "start_time, end_time, execution_source) VALUES(?, ?, ?, ?, ?)";
  sql::Statement stmt(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  if (observation.has_id()) {
    stmt.BindInt64(0, observation.id());
  } else {
    stmt.BindNull(0);
  }
  stmt.BindInt64(1, observation.definition().id());
  stmt.BindInt64(2, observation.start_time());
  stmt.BindInt64(3, observation.end_time());
  stmt.BindInt(4, static_cast<int>(observation.execution_source()));
  if (!stmt.Run()) {
    return std::nullopt;
  }
  return observation.has_id() ? observation.id() : db_.GetLastInsertRowId();
}

bool TaskDatabase::UpdateObservation(int64_t observation_id,
                                     const TaskObservation& observation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "UPDATE task_observations SET definition_id=?, start_time=?, end_time=?, "
      "execution_source=? WHERE observation_id=?";
  sql::Statement stmt(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  stmt.BindInt64(0, observation.definition().id());
  stmt.BindInt64(1, observation.start_time());
  stmt.BindInt64(2, observation.end_time());
  stmt.BindInt(3, static_cast<int>(observation.execution_source()));
  stmt.BindInt64(4, observation_id);
  return stmt.Run();
}

base::flat_map<std::pair<int32_t, std::string>, int64_t>
TaskDatabase::GetParameterIdsForDefinition(int64_t definition_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "SELECT ts.step_index, tp.parameter_key, tp.parameter_id "
      "FROM task_steps ts "
      "JOIN task_parameters tp ON ts.step_id = tp.step_id "
      "WHERE ts.definition_id = ?";
  sql::Statement stmt(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  stmt.BindInt64(0, definition_id);

  base::flat_map<std::pair<int32_t, std::string>, int64_t> parameter_id_map;
  while (stmt.Step()) {
    int32_t step_idx = stmt.ColumnInt(0);
    std::string param_key = stmt.ColumnString(1);
    int64_t parameter_id = stmt.ColumnInt64(2);
    parameter_id_map.emplace(std::make_pair(step_idx, std::move(param_key)),
                             parameter_id);
  }
  return parameter_id_map;
}

bool TaskDatabase::HasValueForParameter(int64_t parameter_id,
                                        int64_t observation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "SELECT 1 FROM task_parameter_values WHERE parameter_id=? AND "
      "observation_id=?";
  sql::Statement stmt(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  stmt.BindInt64(0, parameter_id);
  stmt.BindInt64(1, observation_id);
  return stmt.Step();
}

bool TaskDatabase::AddParameterValue(int64_t parameter_id,
                                     int64_t observation_id,
                                     const std::string& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "INSERT INTO task_parameter_values(parameter_id, observation_id, value) "
      "VALUES(?, ?, ?)";
  sql::Statement stmt(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  stmt.BindInt64(0, parameter_id);
  stmt.BindInt64(1, observation_id);
  stmt.BindString(2, value);
  return stmt.Run();
}

bool TaskDatabase::UpdateParameterValue(int64_t parameter_id,
                                        int64_t observation_id,
                                        const std::string& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "UPDATE task_parameter_values SET value=? WHERE parameter_id=? AND "
      "observation_id=?";
  sql::Statement stmt(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  stmt.BindString(0, value);
  stmt.BindInt64(1, parameter_id);
  stmt.BindInt64(2, observation_id);
  return stmt.Run();
}

std::optional<std::string> TaskDatabase::GetParameterValue(
    int64_t parameter_id,
    int64_t observation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "SELECT value FROM task_parameter_values WHERE parameter_id=? AND "
      "observation_id=?";
  sql::Statement stmt(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  stmt.BindInt64(0, parameter_id);
  stmt.BindInt64(1, observation_id);
  if (stmt.Step()) {
    return stmt.ColumnString(0);
  }
  return std::nullopt;
}



bool TaskDatabase::HasDefinitionWithId(std::optional<int64_t> definition_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!definition_id) {
    return false;
  }
  static constexpr char kCheckSql[] =
      // clang-format off
      "SELECT 1 FROM task_definitions WHERE definition_id=?";
  // clang-format on
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kCheckSql));
  statement.BindInt64(0, *definition_id);
  return statement.Step();
}

bool TaskDatabase::AddShallowTaskDefinition(const TaskDefinition& definition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kInsertSql[] =
      // clang-format off
      "INSERT INTO task_definitions(definition_id,recording_id,title,url,"
      "description) VALUES(?,?,?,?,?)";
  // clang-format on
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  if (definition.has_id()) {
    statement.BindInt64(0, definition.id());
  } else {
    statement.BindNull(0);
  }
  if (definition.has_recording_id()) {
    statement.BindInt64(1, definition.recording_id());
  } else {
    statement.BindNull(1);
  }
  statement.BindString(2, definition.title());
  statement.BindString(3, NormalizeUrl(definition.url()));
  statement.BindString(4, definition.description());
  return statement.Run();
}

bool TaskDatabase::UpdateShallowTaskDefinition(
    int64_t definition_id,
    const TaskDefinition& definition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kUpdateSql[] =
      // clang-format off
      "UPDATE task_definitions SET recording_id=?,title=?,url=?,"
      "description=? WHERE definition_id=?";
  // clang-format on
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kUpdateSql));
  if (definition.has_recording_id()) {
    statement.BindInt64(0, definition.recording_id());
  } else {
    statement.BindNull(0);
  }
  statement.BindString(1, definition.title());
  statement.BindString(2, NormalizeUrl(definition.url()));
  statement.BindString(3, definition.description());
  statement.BindInt64(4, definition_id);
  return statement.Run();
}

bool TaskDatabase::SaveSteps(
    int64_t definition_id,
    ::google::protobuf::RepeatedPtrField<TaskStep> steps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::flat_map<int64_t, int32_t> step_index_for_id =
      GetStepIndicesAndIds(definition_id);

  // Temporarily shift all existing steps to negative step indexes to prevent
  // UNIQUE constraint violations during step reordering.
  if (!ShiftStepIndicesNegative(step_index_for_id)) {
    return false;
  }

  std::vector<int64_t> active_step_ids;
  for (TaskStep& step : steps) {
    std::optional<int64_t> step_id = FindStepIndex(step, step_index_for_id);
    ::google::protobuf::RepeatedPtrField<TaskParameter> parameters =
        std::move(*step.mutable_parameters());
    step.mutable_parameters()->Clear();

    if (step_id) {
      step.set_id(*step_id);
      if (!UpdateStep(*step_id, step)) {
        return false;
      }
    } else {
      if (!AddStep(definition_id, step)) {
        return false;
      }
      step_id = db_.GetLastInsertRowId();
    }
    active_step_ids.push_back(*step_id);

    if (!SaveParameters(*step_id, std::move(parameters))) {
      return false;
    }
  }

  for (const auto& [step_id, step_index] : step_index_for_id) {
    if (std::find(active_step_ids.begin(), active_step_ids.end(), step_id) ==
        active_step_ids.end()) {
      if (!DeleteStepById(step_id)) {
        return false;
      }
    }
  }
  return true;
}

base::flat_map<int64_t, int32_t> TaskDatabase::GetStepIndicesAndIds(
    int64_t definition_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kGetStepsSql[] =
      // clang-format off
      "SELECT step_id,step_index FROM task_steps WHERE definition_id=? "
      "ORDER BY step_index ASC";
  // clang-format on
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kGetStepsSql));
  statement.BindInt64(0, definition_id);

  base::flat_map<int64_t, int32_t> existing_steps;
  while (statement.Step()) {
    existing_steps.emplace(statement.ColumnInt64(0), statement.ColumnInt(1));
  }
  return existing_steps;
}

bool TaskDatabase::ShiftStepIndicesNegative(
    const base::flat_map<int64_t, int32_t>& step_index_for_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kTempShiftSql[] =
      // clang-format off
      "UPDATE task_steps SET step_index=? WHERE step_id=?";
  // clang-format on
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kTempShiftSql));
  for (const auto& [step_id, step_index] : step_index_for_id) {
    statement.Reset(true);
    // Shift each index to a temporary negative range (-1, -2, ...) to prevent
    // UNIQUE(definition_id, step_index) constraint violations when reordering
    // steps prior to committing the final indices.
    statement.BindInt(0, -1 - step_index);
    statement.BindInt64(1, step_id);
    if (!statement.Run()) {
      return false;
    }
  }
  return true;
}

bool TaskDatabase::UpdateStep(int64_t step_id, const TaskStep& step) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kUpdateStepSql[] =
      // clang-format off
      "UPDATE task_steps SET step_index=?,url=?,description=? WHERE "
      "step_id=?";
  // clang-format on
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kUpdateStepSql));
  statement.BindInt(0, step.step_index());
  statement.BindString(1, NormalizeUrl(step.url()));
  statement.BindString(2, step.description());
  statement.BindInt64(3, step_id);
  return statement.Run();
}

bool TaskDatabase::AddStep(int64_t definition_id, const TaskStep& step) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kInsertStepSql[] =
      // clang-format off
      "INSERT INTO task_steps(step_id,definition_id,step_index,url,"
      "description) VALUES(?,?,?,?,?)";
  // clang-format on
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertStepSql));
  statement.BindNull(0);
  statement.BindInt64(1, definition_id);
  statement.BindInt(2, step.step_index());
  statement.BindString(3, NormalizeUrl(step.url()));
  statement.BindString(4, step.description());
  return statement.Run();
}

std::optional<int64_t> TaskDatabase::FindStepIndex(
    const TaskStep& step,
    const base::flat_map<int64_t, int32_t>& step_index_for_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (step.has_id()) {
    for (const auto& [step_id, step_index] : step_index_for_id) {
      if (step_id == step.id()) {
        return step_id;
      }
    }
    DLOG(ERROR) << "Could not find step: " << step.id();
    return std::nullopt;
  }

  for (const auto& [step_id, step_index] : step_index_for_id) {
    if (step_index == step.step_index()) {
      return step_id;
    }
  }
  return std::nullopt;
}

bool TaskDatabase::DeleteStepById(int64_t step_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // 1. Manually delete associated parameters to prevent orphaned rows
  // (since foreign keys are disabled).
  static constexpr char kDeleteParamsSql[] =
      // clang-format off
      "DELETE FROM task_parameters WHERE step_id=?";
  // clang-format on
  sql::Statement delete_params(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteParamsSql));
  delete_params.BindInt64(0, step_id);
  if (!delete_params.Run()) {
    return false;
  }

  // 2. Delete the step itself.
  static constexpr char kDeleteStepSql[] =
      // clang-format off
      "DELETE FROM task_steps WHERE step_id=?";
  // clang-format on
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteStepSql));
  statement.BindInt64(0, step_id);
  return statement.Run();
}

bool TaskDatabase::SaveParameters(
    int64_t step_id,
    ::google::protobuf::RepeatedPtrField<TaskParameter> parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_map<int64_t, std::string> parameter_key_for_id =
      GetParameterIndicesAndKeys(step_id);

  std::vector<int64_t> active_parameter_ids;
  for (const TaskParameter& parameter : parameters) {
    std::optional<int64_t> parameter_id =
        FindParameterId(parameter, parameter_key_for_id);

    if (parameter_id) {
      if (!UpdateParameter(parameter, *parameter_id)) {
        return false;
      }
    } else {
      if (!AddParameter(step_id, parameter)) {
        return false;
      }
      parameter_id = db_.GetLastInsertRowId();
    }
    active_parameter_ids.push_back(*parameter_id);
  }

  for (const auto& [param_id, param_key] : parameter_key_for_id) {
    if (std::find(active_parameter_ids.begin(), active_parameter_ids.end(),
                  param_id) == active_parameter_ids.end()) {
      if (!DeleteParameterById(param_id)) {
        return false;
      }
    }
  }
  return true;
}

base::flat_map<int64_t, std::string> TaskDatabase::GetParameterIndicesAndKeys(
    int64_t step_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kGetParamsSql[] =
      // clang-format off
      "SELECT parameter_id,parameter_key FROM task_parameters "
      "WHERE step_id=? ORDER BY parameter_id ASC";
  // clang-format on
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetParamsSql));
  statement.BindInt64(0, step_id);
  base::flat_map<int64_t, std::string> parameter_key_for_id;
  while (statement.Step()) {
    parameter_key_for_id.emplace(statement.ColumnInt64(0),
                                 statement.ColumnString(1));
  }
  return parameter_key_for_id;
}

bool TaskDatabase::UpdateParameter(const TaskParameter& parameter,
                                   int64_t parameter_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kUpdateParamSql[] =
      // clang-format off
      "UPDATE task_parameters SET parameter_key=?,name=?,type=?,"
      "description=?,extraction_strategy=? WHERE parameter_id=?";
  // clang-format on
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kUpdateParamSql));
  statement.BindString(0, parameter.key());
  statement.BindString(1, parameter.name());
  statement.BindString(2, parameter.type());
  statement.BindString(3, parameter.description());
  if (parameter.has_extraction_strategy()) {
    statement.BindBlob(4, parameter.extraction_strategy().SerializeAsString());
  } else {
    statement.BindNull(4);
  }
  statement.BindInt64(5, parameter_id);
  return statement.Run();
}

bool TaskDatabase::AddParameter(int64_t step_id,
                                const TaskParameter& parameter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kInsertParamSql[] =
      // clang-format off
      "INSERT INTO task_parameters(parameter_id,step_id,parameter_key,name,"
      "type,description,extraction_strategy) VALUES(?,?,?,?,?,?,?)";
  // clang-format on
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertParamSql));
  statement.BindNull(0);
  statement.BindInt64(1, step_id);
  statement.BindString(2, parameter.key());
  statement.BindString(3, parameter.name());
  statement.BindString(4, parameter.type());
  statement.BindString(5, parameter.description());
  if (parameter.has_extraction_strategy()) {
    statement.BindBlob(6, parameter.extraction_strategy().SerializeAsString());
  } else {
    statement.BindNull(6);
  }
  return statement.Run();
}

std::optional<int64_t> TaskDatabase::FindParameterId(
    const TaskParameter& parameter,
    const base::flat_map<int64_t, std::string>& parameter_key_for_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (parameter.has_id()) {
    for (const auto& [param_id, param_key] : parameter_key_for_id) {
      if (param_id == parameter.id()) {
        return param_id;
      }
    }
    DLOG(ERROR) << "Could not find parameter: " << parameter.id();
    return std::nullopt;
  }
  for (const auto& [param_id, param_key] : parameter_key_for_id) {
    if (param_key == parameter.key()) {
      return param_id;
    }
  }
  return std::nullopt;
}

bool TaskDatabase::DeleteParameterById(int64_t parameter_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kDeleteParamSql[] =
      // clang-format off
      "DELETE FROM task_parameters WHERE parameter_id=?";
  // clang-format on
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteParamSql));
  statement.BindInt64(0, parameter_id);
  return statement.Run();
}

}  // namespace record_replay
