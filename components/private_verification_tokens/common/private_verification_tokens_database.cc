// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_database.h"

#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace {

// Version number of the database.
const int kCurrentVersionNumber = 1;

static constexpr char kDatabaseTag[] = "PrivateVerificationTokens";

// clang-format off

static constexpr char kCreatePublicKeyTableSql[] =
  "CREATE TABLE IF NOT EXISTS keys("
      "id INTEGER PRIMARY KEY,"
      "etld_plus_one TEXT NOT NULL,"
      "public_key BLOB NOT NULL,"
      "key_id INTEGER NOT NULL,"
      "expiration INTEGER NOT NULL,"
      "version INTEGER NOT NULL)";

static constexpr char kInsertPublicKeySql[] =
  "INSERT INTO keys("
      "etld_plus_one,public_key,key_id,expiration,version) "
      "VALUES(?,?,?,?,?)";

// clang-format on

}  // namespace

namespace private_verification_tokens {

std::unique_ptr<PrivateVerificationTokensDatabase>
PrivateVerificationTokensDatabase::Create(base::FilePath path_to_database) {
  if (path_to_database.empty()) {
    return nullptr;
  }
  auto database = std::make_unique<sql::Database>(
      sql::DatabaseOptions{}, sql::Database::Tag(kDatabaseTag));
  return base::WrapUnique(new PrivateVerificationTokensDatabase(
      std::move(database), std::move(path_to_database)));
}

PrivateVerificationTokensDatabase::PrivateVerificationTokensDatabase(
    std::unique_ptr<sql::Database> database,
    base::FilePath path_to_database)
    : database_(std::move(database)),
      path_to_database_(std::move(path_to_database)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PrivateVerificationTokensDatabase::~PrivateVerificationTokensDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const base::FilePath& PrivateVerificationTokensDatabase::PathToDatabase()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return path_to_database_;
}

bool PrivateVerificationTokensDatabase::StoreKeys(
    const std::vector<PrivateVerificationTokensPublicKey>& keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return false;
  }

  sql::Transaction transaction(database_.get());
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kInsertPublicKeySql));
  DCHECK(statement.is_valid());
  for (auto const& pk : keys) {
    statement.Reset(true);
    statement.BindString(0, pk.etld_plus_one());
    statement.BindBlob(1, pk.public_key());
    statement.BindInt64(2, pk.key_id());
    statement.BindInt64(3,
                        pk.expiration().ToDeltaSinceWindowsEpoch().InSeconds());
    statement.BindInt64(4, pk.version());
    if (!statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

bool PrivateVerificationTokensDatabase::EnsureDBInitialized() {
  if (database_->is_open()) {
    return true;
  }
  return InitializeDB();
}

bool PrivateVerificationTokensDatabase::InitializeDB() {
  // Using base::Unretained here is safe because the error callback will never
  // be called after the Database instance is destroyed.
  database_->set_error_callback(base::BindRepeating(
      &PrivateVerificationTokensDatabase::DatabaseErrorCallback,
      base::Unretained(this)));

  const base::FilePath dir = path_to_database_.DirName();
  if (!base::CreateDirectory(dir)) {
    DLOG(ERROR) << "Failed to create directory for Private Verification Token "
                   "database";
    return false;
  }
  if (!base::PathIsWritable(dir)) {
    DLOG(ERROR) << "Private Verification Token database directory is not "
                   "writable";
    return false;
  }
  if (!database_->Open(path_to_database_)) {
    DLOG(ERROR) << "Failed to open Private Verification Token database: "
                << database_->GetErrorMessage();
    return false;
  }
  if (!InitializeSchema(/*is_retry =*/false)) {
    database_->Close();
    return false;
  }

  return true;
}

bool PrivateVerificationTokensDatabase::InitializeSchema(bool is_retry) {
  if (!database_->is_open()) {
    return false;
  }

  sql::MetaTable meta_table;

  // Raze and re-initialize the database if the version is not current.
  if (!meta_table.Init(database_.get(), kCurrentVersionNumber,
                       kCurrentVersionNumber) ||
      (meta_table.GetVersionNumber() != kCurrentVersionNumber)) {
    database_->Raze();
    meta_table.Reset();
    if (is_retry) {
      // Things failed the second time and something is wrong with the
      // initialization logic. Return early to avoid an infinite loop.
      DLOG(ERROR) << "Private Verification Token database version not current "
                     "after re-initialization";
      return false;
    }
    return InitializeSchema(/*is_retry =*/true);
  }

  if (!CreateSchema()) {
    return false;
  }

  return true;
}

bool PrivateVerificationTokensDatabase::CreateSchema() {
  return database_->Execute(kCreatePublicKeyTableSql);
}

void PrivateVerificationTokensDatabase::DatabaseErrorCallback(
    int extended_error,
    sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::UmaHistogramSqliteResult("Storage.PrivateVerificationTokens.DBErrors",
                                extended_error);

  if (sql::IsErrorCatastrophic(extended_error)) {
    // Normally this will poison the database, causing any subsequent
    // operations to silently fail without any side effects. However, if
    // RazeAndPoison() is
    // called from the error callback in response to an error raised from within
    // sql::Database::Open, opening the now-razed database will be retried.
    DLOG(ERROR) << "Corrupted database: " << database_->GetErrorMessage();
    database_->RazeAndPoison();
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error)) {
    DLOG(FATAL) << "Unexpected Sqlite error: " << database_->GetErrorMessage();
  }
}

}  // namespace private_verification_tokens
