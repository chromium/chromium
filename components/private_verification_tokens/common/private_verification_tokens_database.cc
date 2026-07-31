// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_database.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/threading/sequence_bound.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "components/private_verification_tokens/common/private_verification_tokens_token.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// Version number of the database.
const int kCurrentVersionNumber = 4;

static constexpr char kDatabaseTag[] = "PrivateVerificationTokens";

// clang-format off
static constexpr char kCreateTokensTableSql[] =
  "CREATE TABLE IF NOT EXISTS tokens("
      "id INTEGER PRIMARY KEY,"
      "issuer TEXT NOT NULL,"
      "key_id INTEGER NOT NULL,"
      "expiration INTEGER NOT NULL,"
      "token BLOB NOT NULL,"
      "redeemed INTEGER NOT NULL DEFAULT 0,"
      "version INTEGER NOT NULL,"
      "creation_time INTEGER NOT NULL)";

static constexpr char kInsertTokenSql[] =
  "INSERT INTO tokens("
      "issuer,key_id,expiration,token,version,creation_time) "
      "VALUES(?,?,?,?,?,?)";

static constexpr char kGetTokenSql[] =
    "SELECT id,issuer,key_id,expiration,token,version,creation_time "
    "FROM tokens WHERE redeemed = 0 AND issuer = ?";

static constexpr char kGetAllTokensSql[] =
    "SELECT id,issuer,key_id,expiration,token,version,creation_time "
    "FROM tokens WHERE redeemed = 0 "
    "GROUP BY issuer";

static constexpr char kSetTokenRedeemedSql[] =
    "UPDATE tokens "
    "SET redeemed = 1 "
    "WHERE id = ?";

static constexpr char kDeleteRedeemedTokensSql[] =
    "DELETE FROM tokens WHERE redeemed = 1";

// SQLite in Chromium has a limit of 32k placeholders per query. We use
// anywhere between 0-2 placeholders for the time range, plus one for
// each origin in the filter. Setting to a conservative 16k should be safe.
static constexpr size_t kDeleteMaximumOriginsPerQuery = 16384;

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

// static
std::unique_ptr<sql::Database>
PrivateVerificationTokensDatabase::CreateSqlDatabase() {
  return std::make_unique<sql::Database>(sql::DatabaseOptions{},
                                         sql::Database::Tag(kDatabaseTag));
}

std::unique_ptr<PrivateVerificationTokensDatabase>
PrivateVerificationTokensDatabase::Create(base::FilePath path_to_database) {
  if (path_to_database.empty()) {
    return nullptr;
  }
  return std::make_unique<PrivateVerificationTokensDatabase>(
      base::PassKey<PrivateVerificationTokensDatabase>(), CreateSqlDatabase(),
      std::move(path_to_database));
}

// static
base::SequenceBound<PrivateVerificationTokensDatabase>
PrivateVerificationTokensDatabase::CreateSequenceBound(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::FilePath path_to_database) {
  if (path_to_database.empty()) {
    return {};
  }
  return base::SequenceBound<PrivateVerificationTokensDatabase>(
      task_runner, base::PassKey<PrivateVerificationTokensDatabase>(),
      CreateSqlDatabase(), std::move(path_to_database));
}

PrivateVerificationTokensDatabase::PrivateVerificationTokensDatabase(
    base::PassKey<PrivateVerificationTokensDatabase>,
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
    statement.BindString(0, token.issuer().Serialize());
    statement.BindInt64(1, token.key_id());
    statement.BindInt64(
        2, (token.expiration() - base::Time::UnixEpoch()).InSeconds());
    statement.BindBlob(3, token.token());
    statement.BindInt64(4, token.version());
    statement.BindInt64(
        5, (token.creation_time() - base::Time::UnixEpoch()).InSeconds());
    if (!statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

std::optional<TokenWithId> PrivateVerificationTokensDatabase::GetToken(
    const url::Origin& issuer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return std::nullopt;
  }

  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kGetTokenSql));
  DCHECK(statement.is_valid());
  statement.BindString(0, issuer.Serialize());

  if (statement.Step()) {
    int64_t id = statement.ColumnInt64(0);
    std::string issuer_str = statement.ColumnString(1);
    uint32_t key_id = static_cast<uint32_t>(statement.ColumnInt64(2));
    int64_t expiration = statement.ColumnInt64(3);
    SerializedToken token = statement.ColumnBlobAsVector(4);
    uint32_t version = static_cast<uint32_t>(statement.ColumnInt64(5));
    int64_t creation_time = statement.ColumnInt64(6);

    url::Origin read_issuer = url::Origin::Create(GURL(issuer_str));

    return TokenWithId{
        id, PrivateVerificationTokensToken(
                std::move(read_issuer), std::move(token), key_id,
                base::Time::UnixEpoch() + base::Seconds(expiration), version,
                base::Time::UnixEpoch() + base::Seconds(creation_time))};
  }
  if (!statement.Succeeded()) {
    return std::nullopt;
  }
  return std::nullopt;
}

std::map<url::Origin, TokenWithId>
PrivateVerificationTokensDatabase::GetTokensFromEach() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return {};
  }

  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kGetAllTokensSql));
  DCHECK(statement.is_valid());

  std::map<url::Origin, TokenWithId> tokens;
  while (statement.Step()) {
    int64_t id = statement.ColumnInt64(0);
    std::string issuer_str = statement.ColumnString(1);
    uint32_t key_id = static_cast<uint32_t>(statement.ColumnInt64(2));
    int64_t expiration = statement.ColumnInt64(3);
    SerializedToken token = statement.ColumnBlobAsVector(4);
    uint32_t version = static_cast<uint32_t>(statement.ColumnInt64(5));
    int64_t creation_time = statement.ColumnInt64(6);

    url::Origin issuer = url::Origin::Create(GURL(issuer_str));
    tokens.try_emplace(
        issuer, id,
        PrivateVerificationTokensToken(
            issuer, std::move(token), key_id,
            base::Time::UnixEpoch() + base::Seconds(expiration), version,
            base::Time::UnixEpoch() + base::Seconds(creation_time)));
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

bool PrivateVerificationTokensDatabase::DeleteTokens(
    base::Time delete_begin,
    base::Time delete_end,
    base::optional_ref<const std::vector<url::Origin>> issuers) {
  if (!issuers.has_value()) {
    return DeleteTokenBatch(delete_begin, delete_end, {});
  }

  base::span<const url::Origin> issuers_span(*issuers);
  bool result = true;
  for (size_t i = 0; i < issuers_span.size();
       i += kDeleteMaximumOriginsPerQuery) {
    size_t current_chunk_size =
        std::min(kDeleteMaximumOriginsPerQuery, issuers_span.size() - i);
    result &= DeleteTokenBatch(delete_begin, delete_end,
                               issuers_span.subspan(i, current_chunk_size));
  }
  return result;
}

bool PrivateVerificationTokensDatabase::DeleteTokenBatch(
    base::Time delete_begin,
    base::Time delete_end,
    base::span<const url::Origin> issuers) {
  DCHECK(issuers.size() <= kDeleteMaximumOriginsPerQuery);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return false;
  }

  std::vector<std::string> where_clauses;
  if (!delete_begin.is_null() && delete_begin != base::Time::Min()) {
    where_clauses.push_back("creation_time >= ?");
  }
  if (!delete_end.is_null() && delete_end != base::Time::Max()) {
    where_clauses.push_back("creation_time < ?");
  }
  if (!issuers.empty()) {
    std::vector<std::string> placeholders(issuers.size(), "?");
    where_clauses.push_back("issuer IN (" +
                            base::JoinString(placeholders, ", ") + ")");
  }

  std::string sql = "DELETE FROM tokens";
  if (!where_clauses.empty()) {
    sql += " WHERE " + base::JoinString(where_clauses, " AND ");
  }

  sql::Statement statement(database_->GetUniqueStatement(sql));
  DCHECK(statement.is_valid());

  int param_index = 0;
  if (!delete_begin.is_null() && delete_begin != base::Time::Min()) {
    statement.BindInt64(param_index++,
                        (delete_begin - base::Time::UnixEpoch()).InSeconds());
  }
  if (!delete_end.is_null() && delete_end != base::Time::Max()) {
    statement.BindInt64(param_index++,
                        (delete_end - base::Time::UnixEpoch()).InSeconds());
  }
  for (const url::Origin& issuer : issuers) {
    statement.BindString(param_index++, issuer.Serialize());
  }

  DCHECK_EQ(static_cast<size_t>(param_index),
            static_cast<size_t>(std::count(sql.begin(), sql.end(), '?')));

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
  return database_->Execute(kCreateTokensTableSql);
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
