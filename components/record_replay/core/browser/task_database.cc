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
#include "components/record_replay/core/browser/annotation_parsing_utils.h"
#include "components/record_replay/core/browser/parsing_utils.h"
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

  // Open the database file. If this fails, the database remains closed
  // and subsequent AsyncCalls will fail safely/silently via error callback.
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
    std::ignore = db_.Execute("DROP TABLE IF EXISTS Recordings");
    std::ignore = db_.Execute("DROP TABLE IF EXISTS ActivityAnnotations");
    std::ignore = db_.Execute("DROP TABLE IF EXISTS ActivityData");
  }

  if (!CreateRecordingsTable()) {
    DLOG(ERROR) << "Failed to create Recordings table.";
    return;
  }

  if (!CreateActivityAnnotationsTable()) {
    DLOG(ERROR) << "Failed to create ActivityAnnotations table.";
    return;
  }

  if (!CreateActivityDataTable()) {
    DLOG(ERROR) << "Failed to create ActivityData table.";
    return;
  }

  // Intent: Apply any necessary migrations to align schema with code
  // expectations.
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
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "url TEXT,"
      "start_time INTEGER,"
      "name TEXT,"
      "proto BLOB)";
  if (!db_.Execute(kSql)) {
    return false;
  }

  return db_.Execute(
      "CREATE INDEX IF NOT EXISTS recordings_url ON Recordings(url)");
}

bool TaskDatabase::CreateActivityAnnotationsTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.DoesTableExist("ActivityAnnotations")) {
    return true;
  }

  static constexpr char kSql[] =
      "CREATE TABLE ActivityAnnotations("
      "annotation_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "recording_id INTEGER REFERENCES Recordings(id) ON DELETE SET NULL,"
      "target_url TEXT,"
      "proto BLOB)";
  if (!db_.Execute(kSql)) {
    return false;
  }
  return db_.Execute(
      "CREATE INDEX IF NOT EXISTS activity_annotations_url ON "
      "ActivityAnnotations(target_url)");
}

bool TaskDatabase::IsActivityAnnotationsTableEmpty() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "SELECT EXISTS(SELECT 1 FROM ActivityAnnotations)";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  return statement.Step() && statement.ColumnInt(0) == 0;
}

base::expected<std::vector<ActivityAnnotation>, std::string>
TaskDatabase::GetSeedAnnotationsFromJson(const std::string& json_string) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsActivityAnnotationsTableEmpty()) {
    return std::vector<ActivityAnnotation>();
  }

  std::vector<base::Value> values = ParseJSONListOfDicts(json_string);
  std::vector<ActivityAnnotation> seeded_annotations;
  int index = 0;
  for (const base::Value& item : values) {
    if (!item.is_dict()) {
      return base::unexpected(
          base::StrCat({"Activity metadata item at index ",
                        base::NumberToString(index), " is not a dictionary."}));
    }

    // Parse and validate the annotation using modern base::Value::Dict.
    base::expected<ActivityAnnotation, std::string> result =
        ParseAnnotation(item.GetDict());
    if (!result.has_value()) {
      return base::unexpected(
          base::StrCat({"Error in item ", base::NumberToString(index), ": ",
                        result.error()}));
    }

    seeded_annotations.push_back(std::move(result.value()));
    index++;
  }
  return seeded_annotations;
}

base::expected<std::vector<ActivityAnnotation>, std::string>
TaskDatabase::SeedAnnotationsFromFile(const base::FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsActivityAnnotationsTableEmpty()) {
    return std::vector<ActivityAnnotation>();
  }

  std::string json_string;
  if (!base::ReadFileToString(file_path, &json_string)) {
    return base::unexpected(base::StrCat(
        {"Failed to read activity metadata file: ", file_path.AsUTF8Unsafe()}));
  }

  return GetSeedAnnotationsFromJson(json_string);
}

void TaskDatabase::RunSeeding(base::FilePath file_path,
                              std::string feature_json) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Skip if already seeded.
  if (!IsActivityAnnotationsTableEmpty()) {
    return;
  }

  std::string first_error;

  // Try seeding from a local file first to give developer overrides priority.
  if (!file_path.empty()) {
    if (base::expected<std::vector<ActivityAnnotation>, std::string> result =
            SeedAnnotationsFromFile(file_path);
        result.has_value()) {
      for (ActivityAnnotation& annotation : result.value()) {
        std::string url = annotation.url();
        SaveActivityAnnotation(std::nullopt, std::move(annotation),
                               std::move(url), std::nullopt);
      }
      return;
    } else {
      first_error = std::move(result.error());
    }
  }

  // Fall back to Finch seeding if the local file was not specified or failed.
  if (!feature_json.empty()) {
    if (base::expected<std::vector<ActivityAnnotation>, std::string> result =
            GetSeedAnnotationsFromJson(feature_json);
        result.has_value()) {
      for (ActivityAnnotation& annotation : result.value()) {
        std::string url = annotation.url();
        SaveActivityAnnotation(std::nullopt, std::move(annotation),
                               std::move(url), std::nullopt);
      }
      return;
    } else if (first_error.empty()) {
      first_error = std::move(result.error());
    }
  }

  if (!first_error.empty()) {
    DLOG(ERROR) << first_error;
    base::debug::DumpWithoutCrashing();
  }
}

bool TaskDatabase::CreateActivityDataTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.DoesTableExist("ActivityData")) {
    return true;
  }

  static constexpr char kSql[] =
      "CREATE TABLE ActivityData("
      "annotation_id INTEGER PRIMARY KEY,"
      "proto BLOB,"
      "FOREIGN KEY(annotation_id) REFERENCES "
      "ActivityAnnotations(annotation_id) "
      "ON DELETE CASCADE)";
  return db_.Execute(kSql);
}

int64_t TaskDatabase::AddRecording(Recording recording) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "INSERT INTO Recordings(url, start_time, name, proto) "
      "VALUES(?, ?, ?, ?)";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, std::move(recording.url()));
  statement.BindInt64(1, recording.start_time());
  statement.BindString(2, std::move(recording.name()));

  // The ID should not be serialized to the database.
  recording.clear_id();
  statement.BindBlob(3, recording.SerializeAsString());

  if (statement.Run()) {
    return db_.GetLastInsertRowId();
  }
  return -1;
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

void TaskDatabase::SaveActivityAnnotation(std::optional<int64_t> annotation_id,
                                          ActivityAnnotation annotation,
                                          std::string target_url,
                                          std::optional<int64_t> recording_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string normalized_url = NormalizeUrl(target_url);
  annotation.set_url(normalized_url);
  if (recording_id) {
    annotation.set_recording_id(*recording_id);
  }

  static constexpr char kSql[] =
      "INSERT OR REPLACE INTO ActivityAnnotations(annotation_id, recording_id, "
      "target_url, proto) "
      "VALUES(?, ?, ?, ?)";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  if (annotation_id) {
    statement.BindInt64(0, *annotation_id);
  } else {
    statement.BindNull(0);
  }
  if (recording_id) {
    statement.BindInt64(1, *recording_id);
  } else {
    statement.BindNull(1);
  }
  statement.BindString(2, normalized_url);
  statement.BindBlob(3, annotation.SerializeAsString());

  statement.Run();
}

std::optional<ActivityAnnotation> TaskDatabase::GetActivityAnnotation(
    int64_t annotation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "SELECT proto FROM ActivityAnnotations WHERE annotation_id=?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, annotation_id);

  if (statement.Step()) {
    ActivityAnnotation annotation;
    if (annotation.ParseFromString(statement.ColumnBlobAsString(0))) {
      return annotation;
    }
  }

  return std::nullopt;
}

std::vector<std::pair<int64_t, ActivityAnnotation>>
TaskDatabase::GetActivityAnnotationsByUrl(const std::string& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "SELECT annotation_id, proto FROM ActivityAnnotations WHERE target_url=?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, NormalizeUrl(url));

  std::vector<std::pair<int64_t, ActivityAnnotation>> annotations;
  while (statement.Step()) {
    int64_t id = statement.ColumnInt64(0);
    ActivityAnnotation annotation;
    if (annotation.ParseFromString(statement.ColumnBlobAsString(1))) {
      annotations.emplace_back(id, std::move(annotation));
    }
  }
  return annotations;
}

bool TaskDatabase::SaveActivityData(int64_t annotation_id,
                                    const ActivityData& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "INSERT OR REPLACE INTO ActivityData(annotation_id, proto) "
      "VALUES(?, ?)";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, annotation_id);
  statement.BindBlob(1, data.SerializeAsString());

  return statement.Run();
}

std::optional<ActivityData> TaskDatabase::GetActivityData(
    int64_t annotation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "SELECT proto FROM ActivityData WHERE annotation_id=?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, annotation_id);

  if (statement.Step()) {
    ActivityData data;
    if (data.ParseFromString(statement.ColumnBlobAsString(0))) {
      return data;
    }
  }

  return std::nullopt;
}

bool TaskDatabase::DeleteActivityData(int64_t annotation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "DELETE FROM ActivityData WHERE annotation_id=?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, annotation_id);

  return statement.Run();
}

bool TaskDatabase::DeleteActivityAnnotation(int64_t annotation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "DELETE FROM ActivityAnnotations WHERE annotation_id=?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, annotation_id);

  return statement.Run();
}

}  // namespace record_replay
