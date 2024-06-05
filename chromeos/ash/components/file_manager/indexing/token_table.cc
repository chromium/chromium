// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/token_table.h"

#include "base/logging.h"
#include "sql/statement.h"

namespace ash::file_manager {

namespace {

#define TOKEN_TABLE "token_table"
#define TOKEN_ID "token_id"
#define TOKEN_BYTES "token_bytes"
#define TOKEN_INDEX "token_index"

// The statement used to create the token table.
static constexpr char kCreateTokenTableQuery[] =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS " TOKEN_TABLE "("
      TOKEN_ID " INTEGER PRIMARY KEY AUTOINCREMENT,"
      TOKEN_BYTES " TEXT NOT NULL)";
// clang-format on

// The statement used to delete a token from the database by token ID.
static constexpr char kDeleteTokenQuery[] =
    // clang-format off
    "DELETE FROM " TOKEN_TABLE " WHERE " TOKEN_ID "=?";
// clang-format on

// The statement used fetch the ID of the token.
static constexpr char kGetTokenIdQuery[] =
    // clang-format off
    "SELECT " TOKEN_ID " FROM " TOKEN_TABLE " WHERE " TOKEN_BYTES "=?";
// clang-format on

// The statement used fetch the ID of the token.
static constexpr char kGetTokenValueQuery[] =
    // clang-format off
    "SELECT " TOKEN_BYTES " FROM " TOKEN_TABLE " WHERE " TOKEN_ID "=?";
// clang-format on

// The statement used to insert a new token into the table.
static constexpr char kInsertTokenQuery[] =
    // clang-format off
    "INSERT INTO " TOKEN_TABLE "(" TOKEN_BYTES ") VALUES (?) RETURNING "
    TOKEN_ID "";
// clang-format on

// The statement that creates an index on tokens.
static constexpr char kCreateTokenIndexQuery[] =
    // clang-format off
    "CREATE UNIQUE INDEX IF NOT EXISTS " TOKEN_INDEX " ON " TOKEN_TABLE
    "(" TOKEN_BYTES ")";
// clang-format on

// The statement that changes token value from one string to another
static constexpr char kChangeTokenValueQuery[] =
    // clang-format off
   "UPDATE OR ROLLBACK " TOKEN_TABLE " SET " TOKEN_BYTES "=? WHERE "
   TOKEN_BYTES "=? RETURNING " TOKEN_ID;
// clang-format on

}  // namespace

TokenTable::TokenTable(sql::Database* db) : TextTable(db, "" TOKEN_TABLE "") {}
TokenTable::~TokenTable() = default;

int64_t TokenTable::DeleteToken(const std::string& token_bytes) {
  return DeleteValue(token_bytes);
}

std::optional<std::string> TokenTable::GetToken(int64_t token_id) const {
  return GetValue(token_id);
}

int64_t TokenTable::GetTokenId(const std::string& token_bytes) const {
  return GetValueId(token_bytes);
}

int64_t TokenTable::GetOrCreateTokenId(const std::string& token_bytes) {
  return GetOrCreateValueId(token_bytes);
}

int64_t TokenTable::ChangeToken(const std::string& from,
                                const std::string& to) {
  return ChangeValue(from, to);
}

std::unique_ptr<sql::Statement> TokenTable::MakeGetValueIdStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetTokenIdQuery));
}

std::unique_ptr<sql::Statement> TokenTable::MakeGetValueStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetTokenValueQuery));
}

std::unique_ptr<sql::Statement> TokenTable::MakeInsertStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertTokenQuery));
}

std::unique_ptr<sql::Statement> TokenTable::MakeDeleteStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteTokenQuery));
}

std::unique_ptr<sql::Statement> TokenTable::MakeCreateTableStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateTokenTableQuery));
}

std::unique_ptr<sql::Statement> TokenTable::MakeCreateIndexStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateTokenIndexQuery));
}

std::unique_ptr<sql::Statement> TokenTable::MakeChangeValueStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kChangeTokenValueQuery));
}

}  // namespace ash::file_manager
