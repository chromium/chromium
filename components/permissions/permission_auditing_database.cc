// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_auditing_database.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace permissions {

namespace {

// For this database, schema migration is supported for at least 1 year.
// This means we can deprecate old versions that landed more than a year ago.
//
// Version 1: https://crrev.com/c/2435307 - 2020-11-04

// The current number of the database schema.
constexpr int kVersion = 1;

// The lowest version of the database schema such that a legacy code can still
// read/write the current database.
constexpr int kCompatibleVersion = 1;

}  // namespace

PermissionAuditingDatabase::PermissionAuditingDatabase() = default;

PermissionAuditingDatabase::~PermissionAuditingDatabase() = default;

bool PermissionAuditingDatabase::Init(const base::FilePath& path) {
  db_.set_histogram_tag("Permission Auditing Logs");
  if (!db_.Open(path)) {
    return false;
  }
  sql::MetaTable metatable;
  if (!metatable.Init(&db_, kVersion, kCompatibleVersion)) {
    db_.Poison();
    return false;
  }
  if (metatable.GetCompatibleVersionNumber() > kVersion) {
    db_.Poison();
    return false;
  }
  if (!db_.DoesTableExist("uses")) {
    if (!CreateSchema()) {
      db_.Poison();
      return false;
    }
  }
  // TODO(anyone): perform migration if metatable.GetVersionNumber() < kVersion
  return true;
}

bool PermissionAuditingDatabase::CreateSchema() {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }
  if (!db_.Execute("CREATE TABLE uses("
                   "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                   "  origin TEXT NOT NULL,"
                   "  content_setting_type INTEGER NOT NULL,"
                   "  usage_start_time INTEGER NOT NULL,"
                   "  usage_end_time INTEGER NOT NULL,"
                   "  had_user_activation INTEGER NOT NULL,"
                   "  was_foreground INTEGER NOT NULL,"
                   "  had_focus INTEGER NOT NULL"
                   ")")) {
    return false;
  }
  if (!db_.Execute("CREATE UNIQUE INDEX setting_origin_start_time ON "
                   "uses(origin, content_setting_type,"
                   "usage_start_time)")) {
    return false;
  }
  if (!db_.Execute("CREATE INDEX setting_origin_end_time ON "
                   "uses(origin, content_setting_type,"
                   "usage_end_time)")) {
    return false;
  }
  if (!db_.Execute("CREATE INDEX start_time ON "
                   "uses(usage_start_time)")) {
    return false;
  }
  if (!db_.Execute("CREATE INDEX end_time ON "
                   "uses(usage_end_time)")) {
    return false;
  }
  return transaction.Commit();
}

bool PermissionAuditingDatabase::StorePermissionUsage(
    const PermissionUsageSession& session) {
  DCHECK(session.IsValid());
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "INSERT INTO uses(origin, content_setting_type,"
                             "usage_start_time, usage_end_time,"
                             "had_user_activation, was_foreground, had_focus)"
                             "VALUES (?, ?, ?, ?, ?, ?, ?)"));
  statement.BindString(0, session.origin.Serialize());
  statement.BindInt(1, static_cast<int32_t>(session.type));
  statement.BindTime(2, session.usage_start);
  statement.BindTime(3, session.usage_end);
  statement.BindBool(4, session.had_user_activation);
  statement.BindBool(5, session.was_foreground);
  statement.BindBool(6, session.had_focus);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }
  if (!statement.Run()) {
    return false;
  }
  return transaction.Commit();
}

std::vector<PermissionUsageSession>
PermissionAuditingDatabase::GetPermissionUsageHistory(ContentSettingsType type,
                                                      const url::Origin& origin,
                                                      base::Time start_time) {
  DCHECK(!origin.opaque());
  std::vector<PermissionUsageSession> sessions;
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT usage_start_time, usage_end_time, had_user_activation,"
      "was_foreground, had_focus "
      "FROM uses "
      "WHERE origin = ? AND content_setting_type = ? "
      "AND usage_end_time >= ?"));
  statement.BindString(0, origin.Serialize());
  statement.BindInt(1, static_cast<int32_t>(type));
  statement.BindTime(2, start_time.is_null() ? base::Time::Min() : start_time);

  while (statement.Step()) {
    sessions.push_back({.origin = origin,
                        .type = type,
                        .usage_start = statement.ColumnTime(0),
                        .usage_end = statement.ColumnTime(1),
                        .had_user_activation = statement.ColumnBool(2),
                        .was_foreground = statement.ColumnBool(3),
                        .had_focus = statement.ColumnBool(4)});
  }
  return sessions;
}

std::optional<base::Time>
PermissionAuditingDatabase::GetLastPermissionUsageTime(
    ContentSettingsType type,
    const url::Origin& origin) {
  DCHECK(!origin.opaque());
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "SELECT usage_end_time "
                             "FROM uses "
                             "WHERE origin = ? AND content_setting_type = ? "
                             "ORDER BY usage_end_time DESC "
                             "LIMIT 1"));
  statement.BindString(0, origin.Serialize());
  statement.BindInt(1, static_cast<int32_t>(type));
  std::optional<base::Time> last_usage;
  if (statement.Step()) {
    last_usage = statement.ColumnTime(0);
  }
  return last_usage;
}

bool PermissionAuditingDatabase::UpdateEndTime(ContentSettingsType type,
                                               const url::Origin& origin,
                                               base::Time start_time,
                                               base::Time new_end_time) {
  DCHECK(!origin.opaque());
  DCHECK(!start_time.is_null());
  DCHECK(!new_end_time.is_null());
  DCHECK_LE(start_time, new_end_time);
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "UPDATE uses "
                             "SET usage_end_time = ? "
                             "WHERE origin = ? AND content_setting_type = ? "
                             "AND usage_start_time = ?"));
  statement.BindTime(0, new_end_time);
  statement.BindString(1, origin.Serialize());
  statement.BindInt(2, static_cast<int32_t>(type));
  statement.BindTime(3, start_time);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }
  if (!statement.Run()) {
    return false;
  }
  return transaction.Commit();
}

bool PermissionAuditingDatabase::DeleteSessionsBetween(base::Time start_time,
                                                       base::Time end_time) {
  std::vector<int> ids;
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "DELETE FROM uses "
                             "WHERE usage_start_time BETWEEN ? AND ? "
                             "OR usage_end_time BETWEEN ? AND ?"));
  statement.BindTime(0, start_time.is_null() ? base::Time::Min() : start_time);
  statement.BindTime(1, end_time.is_null() ? base::Time::Max() : end_time);
  statement.BindTime(2, start_time.is_null() ? base::Time::Min() : start_time);
  statement.BindTime(3, end_time.is_null() ? base::Time::Max() : end_time);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }
  if (!statement.Run()) {
    return false;
  }
  return transaction.Commit();
}

}  // namespace permissions
