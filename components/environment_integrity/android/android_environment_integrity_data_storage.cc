// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/environment_integrity/android/android_environment_integrity_data_storage.h"

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/sqlite/sqlite3.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// Version number of the database.
const int kCurrentVersionNumber = 1;

}  // namespace

namespace environment_integrity {

AndroidEnvironmentIntegrityDataStorage::AndroidEnvironmentIntegrityDataStorage(
    const base::FilePath& path_to_database)
    : path_to_database_(path_to_database), db_(sql::DatabaseOptions{}) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AndroidEnvironmentIntegrityDataStorage::
    ~AndroidEnvironmentIntegrityDataStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool AndroidEnvironmentIntegrityDataStorage::EnsureDBInitialized() {
  if (db_.is_open()) {
    return true;
  }
  return InitializeDB();
}

bool AndroidEnvironmentIntegrityDataStorage::InitializeDB() {
  db_.set_error_callback(base::BindRepeating(
      &AndroidEnvironmentIntegrityDataStorage::DatabaseErrorCallback,
      base::Unretained(this)));
  db_.set_histogram_tag("EnvironmentIntegrity");

  if (path_to_database_.empty()) {
    if (!db_.OpenInMemory()) {
      DLOG(ERROR)
          << "Failed to create in-memory environment integrity database: "
          << db_.GetErrorMessage();
      return false;
    }
  } else {
    const base::FilePath dir = path_to_database_.DirName();

    if (!base::CreateDirectory(dir)) {
      DLOG(ERROR)
          << "Failed to create directory for environment integrity database";
      return false;
    }
    if (!db_.Open(path_to_database_)) {
      DLOG(ERROR) << "Failed to open environment integrity database: "
                  << db_.GetErrorMessage();
      return false;
    }
  }

  if (!InitializeSchema()) {
    db_.Close();
    return false;
  }

  DCHECK(sql::MetaTable::DoesTableExist(&db_));
  DCHECK(db_.DoesTableExist("environment_integrity_handles"));
  return true;
}

bool AndroidEnvironmentIntegrityDataStorage::InitializeSchema() {
  if (!db_.is_open()) {
    return false;
  }

  sql::MetaTable meta_table;
  if (!meta_table.Init(&db_, kCurrentVersionNumber, kCurrentVersionNumber)) {
    return false;
  }

  if (!CreateSchema()) {
    return false;
  }

  // This is the first code version. No database version is expected to be
  // smaller. Fail when this happens.
  if (meta_table.GetVersionNumber() < kCurrentVersionNumber) {
    return false;
  }

  // This is possible with code reverts. The DB will never work until Chrome
  // is re-upgraded. Assume the user will continue using this Chrome version
  // and raze the DB to get the feature working.
  if (meta_table.GetVersionNumber() > kCurrentVersionNumber) {
    db_.Raze();
    meta_table.Reset();
    return InitializeSchema();
  }

  return true;
}

bool AndroidEnvironmentIntegrityDataStorage::CreateSchema() {
  static constexpr char kDEnvironmentIntegrityHandlesTableSql[] =
      // clang-format off
      "CREATE TABLE IF NOT EXISTS environment_integrity_handles("
          "origin TEXT PRIMARY KEY NOT NULL,"
          "handle INTEGER NOT NULL)";
  // clang-format on
  return db_.Execute(kDEnvironmentIntegrityHandlesTableSql);
}

void AndroidEnvironmentIntegrityDataStorage::DatabaseErrorCallback(
    int extended_error,
    sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::UmaHistogramSqliteResult("Storage.EnvironmentIntegrity.DBErrors",
                                extended_error);

  if (sql::IsErrorCatastrophic(extended_error)) {
    // Normally this will poison the database, causing any subsequent operations
    // to silently fail without any side effects. However, if RazeAndPoison() is
    // called from the error callback in response to an error raised from within
    // sql::Database::Open, opening the now-razed database will be retried.
    db_.RazeAndPoison();
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error)) {
    DLOG(FATAL) << db_.GetErrorMessage();
  }
}

absl::optional<int64_t> AndroidEnvironmentIntegrityDataStorage::GetHandle(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return absl::nullopt;
  }

  static constexpr char kGetHandleSql[] =
      // clang-format off
      "SELECT handle "
          "FROM environment_integrity_handles "
          "WHERE origin=?";
  // clang-format on

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetHandleSql));

  if (!statement.is_valid()) {
    DLOG(ERROR) << "GetHandle SQL statement did not compile.";
    return absl::nullopt;
  }

  statement.BindString(0, origin.Serialize());

  if (!statement.Step() || !statement.Succeeded()) {
    return absl::nullopt;
  }

  return statement.ColumnInt64(0);
}

void AndroidEnvironmentIntegrityDataStorage::SetHandle(
    const url::Origin& origin,
    int64_t handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  static constexpr char kSetHandleSql[] =
      // clang-format off
      "INSERT OR REPLACE INTO environment_integrity_handles(origin,handle) "
          "VALUES(?,?)";
  // clang-format on

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSetHandleSql));

  if (!statement.is_valid()) {
    DLOG(ERROR) << "SetHandle SQL statement did not compile.";
    return;
  }
  statement.BindString(0, origin.Serialize());
  statement.BindInt64(1, handle);

  if (!statement.Run()) {
    DLOG(ERROR) << "Could not set environment integrity handle: "
                << db_.GetErrorMessage();
  }
}

absl::optional<std::vector<url::Origin>> DoGetAllOrigins(sql::Database& db) {
  std::vector<url::Origin> result;
  sql::Statement load(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT DISTINCT origin "
                            "FROM environment_integrity_handles"));
  if (!load.is_valid()) {
    DLOG(ERROR) << "GetAllOrigins SQL statement did not compile: "
                << db.GetErrorMessage();
    return absl::nullopt;
  }

  while (load.Step()) {
    url::Origin origin = url::Origin::Create(GURL(load.ColumnString(0)));
    result.push_back(origin);
  }
  if (!load.Succeeded()) {
    return absl::nullopt;
  }
  return result;
}

bool DoDeleteForOrigin(sql::Database& db, url::Origin origin) {
  sql::Statement delete_statement(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM environment_integrity_handles "
                            "WHERE origin=?"));
  if (!delete_statement.is_valid()) {
    return false;
  }

  delete_statement.BindString(0, origin.Serialize());
  return delete_statement.Run();
}

bool DoClearData(
    sql::Database& db,
    content::StoragePartition::StorageKeyMatcherFunction storage_key_matcher) {
  sql::Transaction transaction(&db);

  if (!transaction.Begin()) {
    return false;
  }

  std::vector<url::Origin> affected_origins;
  absl::optional<std::vector<url::Origin>> maybe_all_origins =
      DoGetAllOrigins(db);

  if (!maybe_all_origins) {
    return false;
  }
  for (const url::Origin& origin : maybe_all_origins.value()) {
    if (storage_key_matcher.is_null() ||
        storage_key_matcher.Run(blink::StorageKey::CreateFirstParty(origin))) {
      affected_origins.push_back(origin);
    }
  }

  for (const auto& affected_origin : affected_origins) {
    if (!DoDeleteForOrigin(db, affected_origin)) {
      return false;
    }
  }

  return transaction.Commit();
}

void AndroidEnvironmentIntegrityDataStorage::ClearData(
    content::StoragePartition::StorageKeyMatcherFunction storage_key_matcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  if (!DoClearData(db_, storage_key_matcher)) {
    DLOG(ERROR) << "Could not delete environment integrity data: "
                << db_.GetErrorMessage();
  }
}

}  // namespace environment_integrity
