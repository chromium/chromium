// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_database.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_service.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace user_annotations {

inline constexpr base::FilePath::CharType kUserAnnotationsName[] =
    FILE_PATH_LITERAL("UserAnnotations");

// These database versions should roll together unless we develop migrations.
constexpr int kLowestSupportedDatabaseVersion = 1;
constexpr int kCurrentDatabaseVersion = 1;

namespace {

[[nodiscard]] bool CreateTable(sql::Database& db) {
  static constexpr char kSqlCreateTablePassages[] =
      "CREATE TABLE IF NOT EXISTS entries("
      // The ID of the entry.
      "entry_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      // The key of the entry.
      "key VARCHAR NOT NULL,"
      // An opaque encrypted blob of value.
      "value BLOB NOT NULL,"
      // The time the entry was created.
      "creation_time INTEGER NOT NULL,"
      // The time the entry was last modified.
      "last_modified_time INTEGER NOT NULL);";

  return db.Execute(kSqlCreateTablePassages);
}

}  // namespace

UserAnnotationsDatabase::UserAnnotationsDatabase(
    const base::FilePath& storage_dir,
    os_crypt_async::Encryptor encryptor)
    : encryptor_(std::move(encryptor)) {
  InitInternal(storage_dir);
  // TODO(b:361696651): Record the DB init status.
}

UserAnnotationsDatabase::~UserAnnotationsDatabase() = default;

sql::InitStatus UserAnnotationsDatabase::InitInternal(
    const base::FilePath& storage_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.set_histogram_tag("UserAnnotations");

  base::FilePath db_file_path = storage_dir.Append(kUserAnnotationsName);
  if (!db_.Open(db_file_path)) {
    return sql::InitStatus::INIT_FAILURE;
  }

  // Raze old incompatible databases.
  if (sql::MetaTable::RazeIfIncompatible(&db_, kLowestSupportedDatabaseVersion,
                                         kCurrentDatabaseVersion) ==
      sql::RazeIfIncompatibleResult::kFailed) {
    return sql::InitStatus::INIT_FAILURE;
  }

  // Wrap initialization in a transaction to make it atomic.
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return sql::InitStatus::INIT_FAILURE;
  }

  // Initialize the current version meta table. Safest to leave the compatible
  // version equal to the current version - unless we know we're making a very
  // safe backwards-compatible schema change.
  sql::MetaTable meta_table;
  if (!meta_table.Init(&db_, kCurrentDatabaseVersion,
                       /*compatible_version=*/kCurrentDatabaseVersion)) {
    return sql::InitStatus::INIT_FAILURE;
  }
  if (meta_table.GetCompatibleVersionNumber() > kCurrentDatabaseVersion) {
    return sql::INIT_TOO_NEW;
  }

  if (!CreateTable(db_)) {
    return sql::INIT_FAILURE;
  }

  if (!transaction.Commit()) {
    return sql::INIT_FAILURE;
  }

  return sql::InitStatus::INIT_OK;
}

UserAnnotationsExecutionResult UserAnnotationsDatabase::UpdateEntries(
    const UserAnnotationsEntries& upserted_entries,
    const std::set<EntryID>& deleted_entry_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return UserAnnotationsExecutionResult::kSqlError;
  }
  auto now_time = base::Time::Now();
  for (const auto& entry : upserted_entries) {
    auto encrypted_value = encryptor_.EncryptString(entry.value());
    if (!encrypted_value) {
      return UserAnnotationsExecutionResult::kCryptError;
    }
    if (entry.entry_id() == 0) {
      // New entry.
      static constexpr char kSqlInsertEntry[] =
          "INSERT INTO entries(key, value, creation_time, "
          "last_modified_time) "
          "VALUES(?,?,?,?)";
      sql::Statement statement(
          db_.GetCachedStatement(SQL_FROM_HERE, kSqlInsertEntry));
      statement.BindString(0, entry.key());
      statement.BindBlob(1, *encrypted_value);
      statement.BindTime(2, now_time);
      statement.BindTime(3, now_time);
      if (!statement.Run()) {
        return UserAnnotationsExecutionResult::kSqlError;
      }
    } else {
      static constexpr char kSqlUpdateEntry[] =
          "UPDATE entries SET key=?, value=?, last_modified_time=? WHERE "
          "entry_id=?";
      sql::Statement statement(
          db_.GetCachedStatement(SQL_FROM_HERE, kSqlUpdateEntry));
      statement.BindString(0, entry.key());
      statement.BindBlob(1, *encrypted_value);
      statement.BindTime(2, now_time);
      statement.BindInt64(3, entry.entry_id());
      if (!statement.Run()) {
        return UserAnnotationsExecutionResult::kSqlError;
      }
    }
  }
  for (const auto& entry_id : deleted_entry_ids) {
    static constexpr char kSqlDeleteEntries[] =
        "DELETE FROM entries WHERE entry_id = ?";
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteEntries));
    statement.BindInt64(0, entry_id);
    if (!statement.Run()) {
      return UserAnnotationsExecutionResult::kSqlError;
    }
  }
  if (!transaction.Commit()) {
    return UserAnnotationsExecutionResult::kSqlError;
  }
  return UserAnnotationsExecutionResult::kSuccess;
}

UserAnnotationsEntryRetrievalResult
UserAnnotationsDatabase::RetrieveAllEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UserAnnotationsEntries entries;
  static constexpr char kSqlSelectAllEntries[] =
      "SELECT entry_id, key, value FROM entries";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectAllEntries));
  while (statement.Step()) {
    optimization_guide::proto::UserAnnotationsEntry entry;
    entry.set_entry_id(statement.ColumnInt64(0));
    entry.set_key(statement.ColumnString(1));
    auto decrypted_value = encryptor_.DecryptData(statement.ColumnBlob(2));
    if (!decrypted_value) {
      return base::unexpected(UserAnnotationsExecutionResult::kCryptError);
    }
    entry.set_value(*decrypted_value);
    entries.push_back(std::move(entry));
  }

  return entries;
}

bool UserAnnotationsDatabase::RemoveEntry(EntryID entry_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Statement delete_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM entries WHERE entry_id=?"));
  delete_statement.BindInt64(0, entry_id);
  return delete_statement.Run();
}

bool UserAnnotationsDatabase::RemoveAllEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Statement delete_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, "DELETE FROM entries"));
  return delete_statement.Run();
}

void UserAnnotationsDatabase::RemoveAnnotationsInRange(
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Statement delete_statement(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "DELETE FROM entries WHERE last_modified_time > ? "
                             "AND last_modified_time < ?"));
  delete_statement.BindTime(0, delete_begin);
  delete_statement.BindTime(1, delete_end);
  delete_statement.Run();
}

int UserAnnotationsDatabase::GetCountOfValuesContainedBetween(base::Time begin,
                                                              base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "SELECT COUNT(DISTINCT(entry_id)) FROM entries "
                             "WHERE last_modified_time > ? "
                             "AND last_modified_time < ?"));
  s.BindTime(0, begin);
  s.BindTime(1, end);

  if (!s.Step()) {
    // This might happen in case of I/O errors. See crbug.com/332263206.
    return 0;
  }
  return s.ColumnInt(0);
}

}  // namespace user_annotations
