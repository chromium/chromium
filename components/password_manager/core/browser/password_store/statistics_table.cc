// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/statistics_table.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <set>

#include "sql/database.h"
#include "sql/statement.h"

namespace password_manager {
namespace {

// Convenience enum for interacting with SQL queries that use all the columns.
enum LoginTableColumns {
  COLUMN_ORIGIN_DOMAIN = 0,
  COLUMN_USERNAME,
  COLUMN_DISMISSALS,
  COLUMN_DATE,
};

// Iterates through all rows of s constructing a stats vector.
std::vector<InteractionsStats> StatementToInteractionsStats(sql::Statement* s) {
  std::vector<InteractionsStats> results;
  while (s->Step()) {
    results.emplace_back();
    results.back().origin_domain = GURL(s->ColumnString(COLUMN_ORIGIN_DOMAIN));
    results.back().username_value = s->ColumnString16(COLUMN_USERNAME);
    results.back().dismissal_count = s->ColumnInt(COLUMN_DISMISSALS);
    results.back().update_time = s->ColumnTime(COLUMN_DATE);
  }

  return results;
}

}  // namespace

StatisticsTable::StatisticsTable() = default;

StatisticsTable::~StatisticsTable() = default;

void StatisticsTable::Init(sql::Database* db) {
  db_ = db;
}

bool StatisticsTable::CreateTableIfNecessary() {
  if (!db_->DoesTableExist("stats")) {
    const char query[] =
        "CREATE TABLE stats ("
        "origin_domain VARCHAR NOT NULL, "
        "username_value VARCHAR, "
        "dismissal_count INTEGER, "
        "update_time INTEGER NOT NULL, "
        "UNIQUE(origin_domain, username_value))";
    if (!db_->Execute(query)) {
      return false;
    }
    const char index[] = "CREATE INDEX stats_origin ON stats(origin_domain)";
    if (!db_->Execute(index)) {
      return false;
    }
  }
  return true;
}

bool StatisticsTable::MigrateToVersion(int version) {
  if (!db_->DoesTableExist("stats")) {
    return true;
  }
  if (version == 16) {
    return db_->Execute("DROP TABLE stats");
  }
  return true;
}

bool StatisticsTable::AddRow(const InteractionsStats& stats) {
  if (!stats.origin_domain.is_valid()) {
    return false;
  }
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO stats "
      "(origin_domain, username_value, dismissal_count, update_time) "
      "VALUES (?, ?, ?, ?)"));
  s.BindString(COLUMN_ORIGIN_DOMAIN, stats.origin_domain.spec());
  s.BindString16(COLUMN_USERNAME, stats.username_value);
  s.BindInt(COLUMN_DISMISSALS, stats.dismissal_count);
  s.BindTime(COLUMN_DATE, stats.update_time);
  return s.Run();
}

bool StatisticsTable::RemoveRow(const GURL& domain) {
  if (!domain.is_valid()) {
    return false;
  }
  sql::Statement s(db_->GetCachedStatement(SQL_FROM_HERE,
                                           "DELETE FROM stats WHERE "
                                           "origin_domain = ? "));
  s.BindString(0, domain.spec());
  return s.Run();
}

std::vector<InteractionsStats> StatisticsTable::GetRows(const GURL& domain) {
  if (!domain.is_valid()) {
    return std::vector<InteractionsStats>();
  }
  const char query[] =
      "SELECT origin_domain, username_value, "
      "dismissal_count, update_time FROM stats WHERE origin_domain == ?";
  sql::Statement s(db_->GetCachedStatement(SQL_FROM_HERE, query));
  s.BindString(0, domain.spec());
  return StatementToInteractionsStats(&s);
}

bool StatisticsTable::RemoveStatsByOriginAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  if (delete_end.is_null()) {
    delete_end = base::Time::Max();
  }

  // All origins.
  if (origin_filter.is_null()) {
    sql::Statement delete_statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "DELETE FROM stats WHERE update_time >= ? AND update_time < ?"));
    delete_statement.BindTime(0, delete_begin);
    delete_statement.BindTime(1, delete_end);

    return delete_statement.Run();
  }

  // Origin filtering.
  sql::Statement select_statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT origin_domain FROM stats "
                              "WHERE update_time >= ? AND update_time < ?"));
  select_statement.BindTime(0, delete_begin);
  select_statement.BindTime(1, delete_end);

  std::set<std::string> origins;
  while (select_statement.Step()) {
    if (!origin_filter.Run(GURL(select_statement.ColumnString(0)))) {
      continue;
    }

    origins.insert(select_statement.ColumnString(0));
  }

  bool success = true;

  for (const std::string& origin : origins) {
    sql::Statement origin_delete_statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "DELETE FROM stats "
        "WHERE origin_domain = ? AND update_time >= ? AND update_time < ?"));
    origin_delete_statement.BindString(0, origin);
    origin_delete_statement.BindTime(1, delete_begin);
    origin_delete_statement.BindTime(2, delete_end);
    success = success && origin_delete_statement.Run();
  }

  return success;
}

int StatisticsTable::GetNumAccounts() {
  sql::Statement select_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, "SELECT COUNT(1) FROM stats"));
  return select_statement.Step() ? select_statement.ColumnInt(0) : 0u;
}

std::vector<InteractionsStats> StatisticsTable::GetAllRowsForTest() {
  static constexpr char query[] =
      "SELECT origin_domain, username_value, "
      "dismissal_count, update_time FROM stats";
  sql::Statement s(db_->GetCachedStatement(SQL_FROM_HERE, query));
  return StatementToInteractionsStats(&s);
}

}  // namespace password_manager
