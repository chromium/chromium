// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_issuer_token_data_storage.h"

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "components/ip_protection/common/ip_protection_issuer_token_fetcher.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/sqlite/sqlite3.h"

namespace {

// Version number of the database.
const int kCurrentVersionNumber = 1;

// clang-format off
static constexpr char kCreateIssuerTokensTableSql[] =
  "CREATE TABLE IF NOT EXISTS tokens("
      "version INTEGER NOT NULL,"
      "u TEXT NOT NULL,"
      "e TEXT NOT NULL,"
      "expiration INTEGER NOT NULL,"
      "num_tokens_with_signal INTEGER NOT NULL,"
      "public_key TEXT NOT NULL)";

static constexpr char kInsertIssuerTokenSql[] =
  "INSERT INTO tokens("
      "version,u,e,expiration,num_tokens_with_signal,public_key) "
      "VALUES(?,?,?,?,?,?)";
// clang-format on

}  // namespace

namespace ip_protection {

IpProtectionIssuerTokenDataStorage::IpProtectionIssuerTokenDataStorage(
    const base::FilePath& path_to_database)
    : path_to_database_(path_to_database),
      db_(sql::DatabaseOptions{},
          sql::Database::Tag("IpProtectionIssuerTokens")) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(!path_to_database_.empty());
}

IpProtectionIssuerTokenDataStorage::~IpProtectionIssuerTokenDataStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool IpProtectionIssuerTokenDataStorage::EnsureDBInitialized() {
  if (db_.is_open()) {
    return true;
  }
  return InitializeDB();
}

bool IpProtectionIssuerTokenDataStorage::InitializeDB() {
  // Using base::Unretained here is safe because the error callback will never
  // be called after the Database instance is destroyed.
  db_.set_error_callback(base::BindRepeating(
      &IpProtectionIssuerTokenDataStorage::DatabaseErrorCallback,
      base::Unretained(this)));

  const base::FilePath dir = path_to_database_.DirName();
  if (!base::CreateDirectory(dir)) {
    DLOG(ERROR) << "Failed to create directory for Issuer Token database";
    return false;
  }
  if (!base::PathIsWritable(dir)) {
    DLOG(ERROR) << "Issuer Token database directory is not writable";
    return false;
  }
  if (!db_.Open(path_to_database_)) {
    DLOG(ERROR) << "Failed to open Issuer Token database: "
                << db_.GetErrorMessage();
    return false;
  }

  if (!InitializeSchema()) {
    db_.Close();
    return false;
  }

  CHECK(sql::MetaTable::DoesTableExist(&db_));
  CHECK(db_.DoesTableExist("tokens"));

  return true;
}
bool IpProtectionIssuerTokenDataStorage::InitializeSchema(bool is_retry) {
  if (!db_.is_open()) {
    return false;
  }

  sql::MetaTable meta_table;
  if (!meta_table.Init(&db_, kCurrentVersionNumber, kCurrentVersionNumber)) {
    return false;
  }

  // Raze and re-initialize the database if the version is not current.
  if (meta_table.GetVersionNumber() != kCurrentVersionNumber) {
    db_.Raze();
    meta_table.Reset();
    // If the database version is still not current after re-initialization,
    // then something went wrong with the initialization logic. Return early to
    // avoid an infinite loop.
    if (is_retry) {
      DLOG(ERROR) << "Issuer Token database version not current after "
                     "re-initialization.";
      return false;
    }
    return InitializeSchema(true);
  }

  if (!CreateSchema()) {
    return false;
  }

  return true;
}

bool IpProtectionIssuerTokenDataStorage::CreateSchema() {
  return db_.Execute(kCreateIssuerTokensTableSql);
}

void IpProtectionIssuerTokenDataStorage::DatabaseErrorCallback(
    int extended_error,
    sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::UmaHistogramSqliteResult("Storage.IpProtectionIssuerTokens.DBErrors",
                                extended_error);

  if (sql::IsErrorCatastrophic(extended_error)) {
    // Normally this will poison the database, causing any subsequent operations
    // to silently fail without any side effects. However, if RazeAndPoison() is
    // called from the error callback in response to an error raised from within
    // sql::Database::Open, opening the now-razed database will be retried.
    DLOG(ERROR) << "Corrupted database: " << db_.GetErrorMessage();
    db_.RazeAndPoison();
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error)) {
    DLOG(FATAL) << "Unexpected Sqlite error: " << db_.GetErrorMessage();
  }
}

void IpProtectionIssuerTokenDataStorage::StoreTokenOutcome(
    TryGetIssuerTokensOutcome outcome) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDBInitialized()) {
    return;
  }

  for (const IssuerToken& token : outcome.tokens) {
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kInsertIssuerTokenSql));

    if (!statement.is_valid()) {
      DLOG(ERROR) << "InsertIssuerToken SQL statement did not compile.";
      return;
    }
    statement.BindInt64(0, token.version);
    statement.BindString(1, token.u);
    statement.BindString(2, token.e);
    statement.BindInt64(3, outcome.expiration_time_seconds);
    statement.BindInt64(4, outcome.num_tokens_with_signal);
    statement.BindString(5, outcome.public_key);

    if (!statement.Run()) {
      DLOG(ERROR) << "Could not insert Issuer Token: " << db_.GetErrorMessage();
    }
  }
}

}  // namespace ip_protection
