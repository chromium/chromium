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
#include "components/private_verification_tokens/common/private_verification_tokens_token.h"
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
static constexpr char kCreateTokensTableSql[] =
  "CREATE TABLE IF NOT EXISTS tokens("
      "id INTEGER PRIMARY KEY,"
      "etld_plus_one TEXT NOT NULL,"
      "key_id INTEGER NOT NULL,"
      "expiration INTEGER NOT NULL,"
      "token BLOB NOT NULL,"
      "redeemed INTEGER NOT NULL DEFAULT 0,"
      "version INTEGER NOT NULL)";

static constexpr char kInsertTokenSql[] =
  "INSERT INTO tokens("
      "etld_plus_one,key_id,expiration,token,version) "
      "VALUES(?,?,?,?,?)";

static constexpr char kGetTokenSql[] =
    "SELECT id,etld_plus_one,key_id,expiration,token,version "
    "FROM tokens WHERE redeemed = 0 AND etld_plus_one = ?";

static constexpr char kGetAllTokensSql[] =
    "SELECT id,etld_plus_one,key_id,expiration,token,version "
    "FROM tokens WHERE redeemed = 0 "
    "GROUP BY etld_plus_one";

static constexpr char kSetTokenRedeemedSql[] =
    "UPDATE tokens "
    "SET redeemed = 1 "
    "WHERE id = ?";

static constexpr char kDeleteRedeemedTokensSql[] =
    "DELETE FROM tokens WHERE redeemed = 1";

static constexpr char kCreatePublicKeyTableSql[] =
  "CREATE TABLE IF NOT EXISTS keys("
      "etld_plus_one TEXT NOT NULL,"
      "public_key BLOB NOT NULL,"
      "key_id INTEGER NOT NULL,"
      "expiration INTEGER NOT NULL,"
      "version INTEGER NOT NULL,"
      "PRIMARY KEY(etld_plus_one, key_id))";

static constexpr char kInsertPublicKeySql[] =
  "INSERT OR REPLACE INTO keys("
      "etld_plus_one,public_key,key_id,expiration,version) "
      "VALUES(?,?,?,?,?)";

static constexpr char kGetAllKeysSql[] =
    "SELECT etld_plus_one,public_key,key_id,expiration,version "
    "FROM keys";

static constexpr char kDeleteKeysForSql[] =
    "DELETE FROM keys WHERE etld_plus_one = ?";

static constexpr char kDeleteKeySql[] =
    "DELETE FROM keys WHERE etld_plus_one = ? AND key_id = ?";

// clang-format on

}  // namespace

namespace private_verification_tokens {

TokenWithId::TokenWithId(int64_t id, PrivateVerificationTokensToken token)
    : id(id), token(std::move(token)) {}
TokenWithId::TokenWithId(const TokenWithId&) = default;
TokenWithId& TokenWithId::operator=(const TokenWithId&) = default;
TokenWithId::TokenWithId(TokenWithId&&) = default;
TokenWithId& TokenWithId::operator=(TokenWithId&&) = default;
TokenWithId::~TokenWithId() = default;

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

bool PrivateVerificationTokensDatabase::StoreTokens(
    const std::vector<PrivateVerificationTokensToken>& tokens) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return false;
  }

  sql::Transaction transaction(database_.get());
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kInsertTokenSql));
  DCHECK(statement.is_valid());
  for (const auto& token : tokens) {
    statement.Reset(true);
    statement.BindString(0, token.etld_plus_one());
    statement.BindInt64(1, token.key_id());
    statement.BindInt64(
        2, token.expiration().ToDeltaSinceWindowsEpoch().InSeconds());
    statement.BindBlob(3, token.token());
    statement.BindInt64(4, token.version());
    if (!statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

std::optional<TokenWithId> PrivateVerificationTokensDatabase::GetToken(
    const std::string& etld_plus_one) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return std::nullopt;
  }

  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kGetTokenSql));
  DCHECK(statement.is_valid());
  statement.BindString(0, etld_plus_one);

  if (statement.Step()) {
    int64_t id = statement.ColumnInt64(0);
    std::string etld_plus_one_str = statement.ColumnString(1);
    uint32_t key_id = static_cast<uint32_t>(statement.ColumnInt64(2));
    int64_t expiration = statement.ColumnInt64(3);
    SerializedToken token = statement.ColumnBlobAsVector(4);
    uint32_t version = static_cast<uint32_t>(statement.ColumnInt64(5));

    return TokenWithId{
        id,
        PrivateVerificationTokensToken(
            std::move(etld_plus_one_str), std::move(token), key_id,
            base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(expiration)),
            version)};
  }
  if (!statement.Succeeded()) {
    return std::nullopt;
  }
  return std::nullopt;
}

std::map<std::string, TokenWithId>
PrivateVerificationTokensDatabase::GetTokensFromEach() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return {};
  }

  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kGetAllTokensSql));
  DCHECK(statement.is_valid());

  std::map<std::string, TokenWithId> tokens;
  while (statement.Step()) {
    int64_t id = statement.ColumnInt64(0);
    std::string etld_plus_one = statement.ColumnString(1);
    uint32_t key_id = static_cast<uint32_t>(statement.ColumnInt64(2));
    int64_t expiration = statement.ColumnInt64(3);
    SerializedToken token = statement.ColumnBlobAsVector(4);
    uint32_t version = static_cast<uint32_t>(statement.ColumnInt64(5));

    tokens.try_emplace(
        etld_plus_one, id,
        PrivateVerificationTokensToken(
            etld_plus_one, std::move(token), key_id,
            base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(expiration)),
            version));
  }
  if (!statement.Succeeded()) {
    return {};
  }
  return tokens;
}

bool PrivateVerificationTokensDatabase::DeleteRedeemedTokens() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return false;
  }
  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kDeleteRedeemedTokensSql));
  DCHECK(statement.is_valid());
  return statement.Run();
}

bool PrivateVerificationTokensDatabase::SetRedeemed(int64_t token_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return false;
  }
  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kSetTokenRedeemedSql));
  DCHECK(statement.is_valid());
  statement.BindInt64(0, token_id);
  return statement.Run();
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

std::vector<PrivateVerificationTokensPublicKey>
PrivateVerificationTokensDatabase::GetKeys() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return {};
  }
  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kGetAllKeysSql));
  DCHECK(statement.is_valid());
  std::vector<PrivateVerificationTokensPublicKey> keys;
  while (statement.Step()) {
    std::string etld_plus_one = statement.ColumnString(0);
    std::vector<uint8_t> public_key = statement.ColumnBlobAsVector(1);
    uint32_t key_id = static_cast<uint32_t>(statement.ColumnInt64(2));
    int64_t expiration = statement.ColumnInt64(3);
    uint32_t version = static_cast<uint32_t>(statement.ColumnInt64(4));
    keys.emplace_back(
        std::move(etld_plus_one), std::move(public_key), key_id,
        base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(expiration)),
        version);
  }
  return keys;
}

bool PrivateVerificationTokensDatabase::RemoveKeysFor(
    const std::string& etld_plus_one) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return false;
  }
  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kDeleteKeysForSql));
  DCHECK(statement.is_valid());
  statement.BindString(0, etld_plus_one);
  return statement.Run();
}

bool PrivateVerificationTokensDatabase::RemoveKey(
    const std::string& etld_plus_one,
    uint32_t key_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return false;
  }
  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kDeleteKeySql));
  DCHECK(statement.is_valid());
  statement.BindString(0, etld_plus_one);
  statement.BindInt64(1, key_id);
  return statement.Run();
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
  return database_->Execute(kCreatePublicKeyTableSql) &&
         database_->Execute(kCreateTokensTableSql);
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
