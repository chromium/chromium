// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/capabilities_database.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace record_replay {

namespace {
// Current version of the database schema.
constexpr int kVersionNumber = 1;

constexpr base::FilePath::StringViewType kCapabilitiesDatabaseFileName =
    FILE_PATH_LITERAL("ReplayCapabilitiesDatabase.db");
}  // namespace

CapabilitiesDatabase::CapabilitiesDatabase()
    : db_(sql::Database::Tag("ReplayCapabilities")) {}
CapabilitiesDatabase::~CapabilitiesDatabase() = default;

void CapabilitiesDatabase::Init(base::FilePath profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.is_open()) {
    return;
  }

  if (!db_.Open(profile_path.Append(kCapabilitiesDatabaseFileName))) {
    return;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch("wipe-recordings")) {
    std::ignore = db_.Execute("DROP TABLE IF EXISTS Recordings");
  }

  if (!CreateRecordingsTable()) {
    return;
  }

  if (!Migrate(GetDatabaseVersion())) {
    return;
  }

  transaction.Commit();
}

int CapabilitiesDatabase::GetDatabaseVersion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db_.GetUniqueStatement("PRAGMA user_version"));
  return statement.Step() ? statement.ColumnInt(0) : 0;
}

bool CapabilitiesDatabase::Migrate(int version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (version < kVersionNumber) {
    // Current version is 1, so no migrations needed yet.
    // In the future, migration steps would go here.
  }

  return db_.Execute(base::StrCat(
      {"PRAGMA user_version = ", base::NumberToString(kVersionNumber)}));
}

bool CapabilitiesDatabase::CreateRecordingsTable() {
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

void CapabilitiesDatabase::AddRecording(Recording recording) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "INSERT INTO Recordings(url, start_time, name, proto) "
      "VALUES(?, ?, ?, ?)";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, std::move(recording.url()));
  statement.BindInt64(1, recording.start_time());
  statement.BindString(2, std::move(recording.name()));
  statement.BindBlob(3, recording.SerializeAsString());

  statement.Run();
}

std::vector<Recording> CapabilitiesDatabase::GetRecordingsByUrl(
    std::string url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kSql[] =
      "SELECT proto FROM Recordings WHERE url=? ORDER BY start_time DESC";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, std::move(url));

  std::vector<Recording> recordings;
  while (statement.Step()) {
    Recording recording;
    if (recording.ParseFromString(statement.ColumnBlobAsString(0))) {
      recordings.push_back(std::move(recording));
    }
  }

  return recordings;
}

}  // namespace record_replay
