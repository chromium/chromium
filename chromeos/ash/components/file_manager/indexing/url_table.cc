// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/url_table.h"

namespace ash::file_manager {

namespace {

#define URL_TABLE "url_table"
#define URL_ID "url_id"
#define URL_SPEC "url_spec"
#define URL_INDEX "url_index"

// The statement used to create the URL table.
static constexpr char kCreateUrlTableQuery[] =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS " URL_TABLE "("
        URL_ID " INTEGER PRIMARY KEY AUTOINCREMENT,"
        URL_SPEC " TEXT NOT NULL)";
// clang-format on

// The statement used to delete a URL from the database by URL ID.
static constexpr char kDeleteUrlQuery[] =
    // clang-format off
    "DELETE FROM " URL_TABLE " WHERE " URL_ID "=?";
// clang-format on

// The statement used fetch the ID of the URL.
static constexpr char kGetUrlIdQuery[] =
    // clang-format off
     "SELECT " URL_ID " FROM " URL_TABLE " WHERE " URL_SPEC "=?";
// clang-format on

// The statement used fetch the ID of the URL.
static constexpr char kGetUrlValueQuery[] =
    // clang-format off
     "SELECT " URL_SPEC " FROM " URL_TABLE " WHERE " URL_ID "=?";
// clang-format on

// The statement used to insert a new URL into the table.
static constexpr char kInsertUrlQuery[] =
    // clang-format off
     "INSERT INTO " URL_TABLE "(" URL_SPEC ") VALUES (?) RETURNING " URL_ID;
// clang-format on

// The statement that creates an index on url_spec column.
static constexpr char kCreateUrlIndexQuery[] =
    // clang-format off
    "CREATE UNIQUE INDEX IF NOT EXISTS " URL_INDEX " ON " URL_TABLE
    "(" URL_SPEC ")";
// clang-format on

// The statement that changes URL from one string to another
static constexpr char kChangeUrlSpecQuery[] =
    // clang-format off
   "UPDATE OR ROLLBACK " URL_TABLE " SET " URL_SPEC "=? WHERE "
   URL_SPEC "=? RETURNING " URL_ID;
// clang-format on

}  // namespace

UrlTable::UrlTable(sql::Database* db) : TextTable(db, "" URL_TABLE "") {}
UrlTable::~UrlTable() = default;

int64_t UrlTable::DeleteUrl(const GURL& url) {
  DCHECK(url.is_valid());
  return DeleteValue(url.spec());
}

int64_t UrlTable::GetUrlId(const GURL& url) const {
  DCHECK(url.is_valid());
  return GetValueId(url.spec());
}

std::optional<std::string> UrlTable::GetUrlSpec(int64_t url_id) const {
  return GetValue(url_id);
}

int64_t UrlTable::GetOrCreateUrlId(const GURL& url) {
  DCHECK(url.is_valid());
  return GetOrCreateValueId(url.spec());
}

int64_t UrlTable::ChangeUrl(const GURL& from, const GURL& to) {
  DCHECK(from.is_valid());
  DCHECK(to.is_valid());
  return ChangeValue(from.spec(), to.spec());
}

std::unique_ptr<sql::Statement> UrlTable::MakeGetValueIdStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetUrlIdQuery));
}

std::unique_ptr<sql::Statement> UrlTable::MakeGetValueStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetUrlValueQuery));
}

std::unique_ptr<sql::Statement> UrlTable::MakeInsertStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertUrlQuery));
}

std::unique_ptr<sql::Statement> UrlTable::MakeDeleteStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteUrlQuery));
}

std::unique_ptr<sql::Statement> UrlTable::MakeCreateTableStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateUrlTableQuery));
}

std::unique_ptr<sql::Statement> UrlTable::MakeCreateIndexStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateUrlIndexQuery));
}

std::unique_ptr<sql::Statement> UrlTable::MakeChangeValueStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kChangeUrlSpecQuery));
}

}  // namespace ash::file_manager
