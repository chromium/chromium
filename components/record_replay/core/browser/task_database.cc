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

void TaskDatabase::Init(base::FilePath profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.is_open()) {
    return;
  }

  db_.set_error_callback(base::BindRepeating([](int error,
                                                sql::Statement* stmt) {
    DLOG(ERROR) << "TaskDatabase SQLite Error: " << error
                << (stmt ? base::StrCat({", SQL: ", stmt->GetSQLStatement()})
                         : "");
  }));

  // Open the database file. If this fails, abort immediately.
  if (!db_.Open(profile_path.Append(kTaskDatabaseFileName))) {
    DLOG(ERROR) << "Failed to open TaskDatabase at: " << profile_path.value();
    return;
  }

  // Enforce referential integrity by enabling foreign key constraints.
  if (!db_.Execute("PRAGMA foreign_keys = ON")) {
    DLOG(ERROR) << "Failed to enable PRAGMA foreign_keys.";
    return;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    DLOG(ERROR) << "Failed to begin transaction for schema creation.";
    return;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch("wipe-recordings")) {
    // Drop tables in the reverse order of dependencies (leaf to root).
    // Since foreign key constraints are active, SQLite would reject dropping
    // parent tables (like Recordings) if their children are still active.
    std::ignore = db_.Execute("DROP TABLE IF EXISTS TaskData");
    std::ignore = db_.Execute("DROP TABLE IF EXISTS TaskDefinitions");
    std::ignore = db_.Execute("DROP TABLE IF EXISTS Recordings");
  }

  if (!CreateRecordingsTable()) {
    DLOG(ERROR) << "Failed to create Recordings table.";
    return;
  }

  if (!CreateTaskDefinitionsTable()) {
    DLOG(ERROR) << "Failed to create TaskDefinitions table.";
    return;
  }

  if (!CreateTaskDataTable()) {
    DLOG(ERROR) << "Failed to create TaskData table.";
    return;
  }

  if (!Migrate(GetDatabaseVersion())) {
    DLOG(ERROR) << "Failed to migrate database.";
    return;
  }

  transaction.Commit();
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
  if (db_.DoesTableExist("Recordings")) {
    return true;
  }

  static constexpr char kSql[] =
      "CREATE TABLE Recordings("
      "id INTEGER PRIMARY KEY,"
      "url TEXT,"
      "start_time INTEGER,"
      "name TEXT,"
      "proto BLOB)";
  if (!db_.Execute(kSql)) {
    return false;
  }

  // Optimize GetRecordingsByUrl queries, which filter by URL and retrieve in
  // descending chronological order. Avoids SQLite in-memory filesorts.
  return db_.Execute(
      "CREATE INDEX IF NOT EXISTS recordings_url_timestamp "
      "ON Recordings(url, start_time DESC)");
}

bool TaskDatabase::CreateTaskDefinitionsTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.DoesTableExist("TaskDefinitions")) {
    return true;
  }

  static constexpr char kSql[] =
      "CREATE TABLE TaskDefinitions("
      "task_definition_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "recording_id INTEGER REFERENCES Recordings(id) ON DELETE SET NULL,"
      "target_url TEXT,"
      "proto BLOB)";
  if (!db_.Execute(kSql)) {
    return false;
  }
  return db_.Execute(
      "CREATE INDEX IF NOT EXISTS task_definitions_url ON "
      "TaskDefinitions(target_url)");
}

bool TaskDatabase::IsTaskDefinitionsTableEmpty() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] = "SELECT EXISTS(SELECT 1 FROM TaskDefinitions)";
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
          base::StrCat({"Task metadata item at index ",
                        base::NumberToString(index), " is not a dictionary."}));
    }

    // Parse and validate the task definition using modern base::Value::Dict.
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
        {"Failed to read task metadata file: ", file_path.AsUTF8Unsafe()}));
  }

  return GetSeedTaskDefinitionsFromJson(json_string);
}

void TaskDatabase::RunSeeding(base::FilePath file_path,
                              std::string feature_json) {
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
            GetSeedTaskDefinitionsFromJson(feature_json);
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

bool TaskDatabase::CreateTaskDataTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.DoesTableExist("TaskData")) {
    return true;
  }

  static constexpr char kSql[] =
      "CREATE TABLE TaskData("
      "task_definition_id INTEGER PRIMARY KEY,"
      "proto BLOB,"
      "FOREIGN KEY(task_definition_id) REFERENCES "
      "TaskDefinitions(task_definition_id) "
      "ON DELETE CASCADE)";
  return db_.Execute(kSql);
}

int64_t TaskDatabase::AddRecording(Recording recording) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Serialize the proto to destructively move its fields afterwards.
  recording.clear_id();
  std::string serialized_proto = recording.SerializeAsString();

  static constexpr char kSql[] =
      "INSERT INTO Recordings(url, start_time, name, proto) "
      "VALUES(?, ?, ?, ?)";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));

  statement.BindString(0, std::move(*recording.mutable_url()));
  statement.BindInt64(1, recording.start_time());
  statement.BindString(2, std::move(*recording.mutable_name()));
  statement.BindBlob(3, std::move(serialized_proto));

  return statement.Run() ? db_.GetLastInsertRowId() : -1;
}

std::vector<Recording> TaskDatabase::GetRecordingsByUrl(std::string url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "SELECT id, proto FROM Recordings WHERE url=? ORDER BY start_time DESC";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, std::move(url));

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

void TaskDatabase::SaveTaskDefinition(std::optional<int64_t> task_definition_id,
                                      TaskDefinition task_definition,
                                      std::string target_url,
                                      std::optional<int64_t> recording_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string normalized_url = NormalizeUrl(target_url);
  task_definition.set_url(normalized_url);
  if (recording_id) {
    task_definition.set_recording_id(*recording_id);
  }

  static constexpr char kSql[] =
      "INSERT OR REPLACE INTO TaskDefinitions(task_definition_id, "
      "recording_id, "
      "target_url, proto) "
      "VALUES(?, ?, ?, ?)";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  if (task_definition_id) {
    statement.BindInt64(0, *task_definition_id);
  } else {
    statement.BindNull(0);
  }
  if (recording_id) {
    statement.BindInt64(1, *recording_id);
  } else {
    statement.BindNull(1);
  }
  statement.BindString(2, normalized_url);
  statement.BindBlob(3, task_definition.SerializeAsString());

  statement.Run();
}

std::optional<TaskDefinition> TaskDatabase::GetTaskDefinition(
    int64_t task_definition_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "SELECT proto FROM TaskDefinitions WHERE task_definition_id=?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, task_definition_id);

  if (statement.Step()) {
    TaskDefinition task_definition;
    if (task_definition.ParseFromString(statement.ColumnBlobAsString(0))) {
      return task_definition;
    }
  }

  return std::nullopt;
}

std::vector<std::pair<int64_t, TaskDefinition>>
TaskDatabase::GetTaskDefinitionsByUrl(const std::string& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "SELECT task_definition_id, proto FROM TaskDefinitions WHERE "
      "target_url=?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, NormalizeUrl(url));

  std::vector<std::pair<int64_t, TaskDefinition>> task_definitions;
  while (statement.Step()) {
    int64_t id = statement.ColumnInt64(0);
    TaskDefinition task_definition;
    if (task_definition.ParseFromString(statement.ColumnBlobAsString(1))) {
      task_definitions.emplace_back(id, std::move(task_definition));
    }
  }
  return task_definitions;
}

bool TaskDatabase::SaveTaskData(int64_t task_definition_id,
                                const TaskData& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "INSERT OR REPLACE INTO TaskData(task_definition_id, proto) "
      "VALUES(?, ?)";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, task_definition_id);
  statement.BindBlob(1, data.SerializeAsString());

  return statement.Run();
}

std::optional<TaskData> TaskDatabase::GetTaskData(int64_t task_definition_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "SELECT proto FROM TaskData WHERE task_definition_id=?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, task_definition_id);

  if (statement.Step()) {
    TaskData data;
    if (data.ParseFromString(statement.ColumnBlobAsString(0))) {
      return data;
    }
  }

  return std::nullopt;
}

bool TaskDatabase::DeleteTaskData(int64_t task_definition_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "DELETE FROM TaskData WHERE task_definition_id=?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, task_definition_id);

  return statement.Run();
}

bool TaskDatabase::DeleteTaskDefinition(int64_t task_definition_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "DELETE FROM TaskDefinitions WHERE task_definition_id=?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, task_definition_id);

  return statement.Run();
}

void TaskDatabase::SaveSeededTaskDefinitions(
    std::vector<TaskDefinition> task_definitions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Transaction transaction(&db_);
  if (transaction.Begin()) {
    for (TaskDefinition& task_definition : task_definitions) {
      std::string url = task_definition.url();
      SaveTaskDefinition(std::nullopt, std::move(task_definition),
                         std::move(url), std::nullopt);
    }
    std::ignore = transaction.Commit();
  }
}

}  // namespace record_replay
