// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/compromised_credentials_table.h"

#include "components/password_manager/core/browser/sql_table_builder.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace password_manager {
namespace {

constexpr char kCompromisedCredentialsTableName[] = "compromised_credentials";

// Represents columns of the compromised credentials table. Used with SQL
// queries that use all the columns.
enum class CompromisedCredentialsTableColumn {
  kUrl,
  kUsername,
  kCreateTime,
  kCompromiseType,
  kMaxValue = kCompromiseType
};

// Casts the compromised credentials table column enum to its integer value.
int GetColumnNumber(CompromisedCredentialsTableColumn column) {
  return static_cast<int>(column);
}

// Teaches |builder| about the different DB schemes in different versions.
void InitializeCompromisedCredentialsBuilder(SQLTableBuilder* builder) {
  // Version 0.
  builder->AddColumnToUniqueKey("url", "VARCHAR NOT NULL");
  builder->AddColumnToUniqueKey("username", "VARCHAR NOT NULL");
  builder->AddColumn("create_time", "INTEGER NOT NULL");
  builder->AddColumnToUniqueKey("compromise_type", "INTEGER NOT NULL");
  builder->AddIndex("compromised_credentials_index",
                    {"url", "username", "compromise_type"});
  builder->SealVersion();
}

// Returns a compromised credentials vector from the SQL statement.
std::vector<CompromisedCredentials> StatementToCompromisedCredentials(
    sql::Statement* s) {
  std::vector<CompromisedCredentials> results;
  while (s->Step()) {
    GURL url(s->ColumnString(
        GetColumnNumber(CompromisedCredentialsTableColumn::kUrl)));
    base::string16 username = s->ColumnString16(
        GetColumnNumber(CompromisedCredentialsTableColumn::kUsername));
    base::Time create_time = base::Time::FromDeltaSinceWindowsEpoch(
        (base::TimeDelta::FromMicroseconds(s->ColumnInt64(
            GetColumnNumber(CompromisedCredentialsTableColumn::kCreateTime)))));
    CompromiseType compromise_type = static_cast<CompromiseType>(s->ColumnInt64(
        GetColumnNumber(CompromisedCredentialsTableColumn::kCompromiseType)));
    results.push_back(CompromisedCredentials(
        std::move(url), std::move(username), create_time, compromise_type));
  }
  return results;
}

}  // namespace

bool operator==(const CompromisedCredentials& lhs,
                const CompromisedCredentials& rhs) {
  return lhs.url == rhs.url && lhs.username == rhs.username &&
         lhs.create_time == rhs.create_time &&
         lhs.compromise_type == rhs.compromise_type;
}

void CompromisedCredentialsTable::Init(sql::Database* db) {
  db_ = db;
}

bool CompromisedCredentialsTable::CreateTableIfNecessary() {
  if (db_->DoesTableExist(kCompromisedCredentialsTableName))
    return true;

  SQLTableBuilder builder(kCompromisedCredentialsTableName);
  InitializeCompromisedCredentialsBuilder(&builder);
  return builder.CreateTable(db_);
}

bool CompromisedCredentialsTable::AddRow(
    const CompromisedCredentials& compromised_credentials) {
  if (!compromised_credentials.url.is_valid())
    return false;
  sql::Statement s(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "INSERT OR IGNORE INTO compromised_credentials "
                              "(url, username, create_time, compromise_type) "
                              "VALUES (?, ?, ?, ?)"));
  s.BindString(GetColumnNumber(CompromisedCredentialsTableColumn::kUrl),
               compromised_credentials.url.spec());
  s.BindString16(GetColumnNumber(CompromisedCredentialsTableColumn::kUsername),
                 compromised_credentials.username);
  s.BindInt64(GetColumnNumber(CompromisedCredentialsTableColumn::kCreateTime),
              compromised_credentials.create_time.ToDeltaSinceWindowsEpoch()
                  .InMicroseconds());
  s.BindInt64(
      GetColumnNumber(CompromisedCredentialsTableColumn::kCompromiseType),
      static_cast<int>(compromised_credentials.compromise_type));
  return s.Run();
}

bool CompromisedCredentialsTable::RemoveRow(const GURL& url,
                                            const base::string16& username) {
  if (!url.is_valid())
    return false;
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM compromised_credentials WHERE url = ? AND username = ? "));
  s.BindString(0, url.spec());
  s.BindString16(1, username);
  return s.Run();
}

bool CompromisedCredentialsTable::RemoveRowsByUrlAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time remove_begin,
    base::Time remove_end) {
  const int64_t remove_begin_us =
      remove_begin.ToDeltaSinceWindowsEpoch().InMicroseconds();
  const int64_t remove_end_us =
      remove_end.ToDeltaSinceWindowsEpoch().InMicroseconds();

  // If |url_filter| is null, remove all records in given date range.
  if (!url_filter) {
    sql::Statement s(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "DELETE FROM compromised_credentials WHERE "
                                "create_time >= ? AND create_time < ?"));
    s.BindInt64(0, remove_begin_us);
    s.BindInt64(1, remove_end_us);
    return s.Run();
  }

  // Otherwise, filter urls.
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT DISTINCT url FROM compromised_credentials WHERE "
      "create_time >= ? AND create_time < ?"));
  s.BindInt64(0, remove_begin_us);
  s.BindInt64(1, remove_end_us);

  std::vector<std::string> urls;
  while (s.Step()) {
    std::string url = s.ColumnString(0);
    if (url_filter.Run(GURL(url))) {
      urls.push_back(std::move(url));
    }
  }

  bool success = true;
  for (const std::string& url : urls) {
    sql::Statement s(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "DELETE FROM compromised_credentials WHERE url = ? "
        "AND create_time >= ? AND create_time < ?"));
    s.BindString(0, url);
    s.BindInt64(1, remove_begin_us);
    s.BindInt64(2, remove_end_us);
    success = success && s.Run();
  }
  return success;
}

std::vector<CompromisedCredentials> CompromisedCredentialsTable::GetAllRows() {
  static constexpr char query[] = "SELECT * FROM compromised_credentials";
  sql::Statement s(db_->GetCachedStatement(SQL_FROM_HERE, query));
  return StatementToCompromisedCredentials(&s);
}

}  // namespace password_manager
