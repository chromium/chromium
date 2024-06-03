// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_url_table.h"

#include <utility>

#include "base/containers/span.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "components/database_utils/url_converter.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace segmentation_platform {

UkmUrlTable::UkmUrlTable(sql::Database* db) : db_(db) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(db_);
}

UkmUrlTable::~UkmUrlTable() = default;

// static
std::string UkmUrlTable::GetDatabaseUrlString(const GURL& url) {
  return database_utils::GurlToDatabaseUrl(url);
}

// static
UrlId UkmUrlTable::GenerateUrlId(const GURL& url) {
  // Converts the 8-byte prefix of an MD5 hash into a int64_t value. This
  // hashing scheme is architecture dependent.
  std::string db_url = GetDatabaseUrlString(url);
  base::MD5Digest digest;
  base::MD5Sum(base::as_byte_span(db_url), &digest);
  return UrlId::FromUnsafeValue(
      base::I64FromLittleEndian(base::span(digest.a).first<8u>()));
}

bool UkmUrlTable::InitTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_->DoesTableExist(kTableName)) {
    if (!db_->DoesColumnExist(kTableName, "profile_id")) {
      // Old versions don't have the profile_id column, we modify the table to
      // add that field.
      return db_->Execute(
          "ALTER TABLE urls "
          "ADD COLUMN profile_id TEXT");
    }
    return true;
  }

  static constexpr char kCreateTableQuery[] =
      // clang-format off
      "CREATE TABLE urls("
        "url_id INTEGER PRIMARY KEY NOT NULL,"
        "url TEXT NOT NULL,"
        "last_timestamp INTEGER NOT NULL,"
        "counter INTEGER,"
        "title TEXT,"
        "profile_id TEXT)";
  // clang-format on
  return db_->Execute(kCreateTableQuery);
}

bool UkmUrlTable::IsUrlInTable(UrlId url_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kGetUrlQuery[] = "SELECT 1 FROM urls WHERE url_id=?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetUrlQuery));
  statement.BindInt64(0, url_id.GetUnsafeValue());
  return statement.Step();
}

bool UkmUrlTable::WriteUrl(const GURL& url,
                           UrlId url_id,
                           base::Time timestamp,
                           const std::string& profile_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kWriteQuery[] =
      "INSERT INTO urls(url_id,url,last_timestamp, profile_id) VALUES(?,?,?,?)";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kWriteQuery));
  statement.BindInt64(0, url_id.GetUnsafeValue());
  statement.BindString(1, database_utils::GurlToDatabaseUrl(url));
  statement.BindTime(2, timestamp);
  statement.BindString(3, profile_id);
  return statement.Run();
}

bool UkmUrlTable::UpdateUrlTimestamp(UrlId url_id, base::Time timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kWriteQuery[] =
      "UPDATE urls SET last_timestamp=? WHERE url_id=?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kWriteQuery));
  statement.BindTime(0, timestamp);
  statement.BindInt64(1, url_id.GetUnsafeValue());
  return statement.Run();
}

bool UkmUrlTable::RemoveUrls(const std::vector<UrlId>& urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kDeleteQuery[] = "DELETE FROM urls WHERE url_id=?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteQuery));
  bool success = true;
  for (UrlId url_id : urls) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, url_id.GetUnsafeValue());
    if (!statement.Run())
      success = false;
  }
  return success;
}

bool UkmUrlTable::DeleteUrlsBeforeTimestamp(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Delete the metrics.
  static constexpr char kDeleteoldEntries[] =
      "DELETE FROM urls WHERE last_timestamp<=?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteoldEntries));
  statement.BindTime(0, time);
  return statement.Run();
}

}  // namespace segmentation_platform
