// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_database.h"

#include <cstdint>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "sql/error_delegate_util.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace critical_actions {

namespace {

constexpr int kCurrentVersion = 1;
constexpr int kCompatibleVersion = 1;

sql::DatabaseOptions GetDatabaseOptions() {
  sql::DatabaseOptions options;
  options.set_page_size(4096);
  options.set_cache_size(32);
  return options;
}

}  // namespace

CriticalActionDatabase::CriticalActionDatabase(const base::FilePath& db_path)
    : db_path_(db_path),
      db_(GetDatabaseOptions(), sql::Database::Tag("CriticalActions")) {}

CriticalActionDatabase::~CriticalActionDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool CriticalActionDatabase::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.set_error_callback(base::BindRepeating(
      &CriticalActionDatabase::DatabaseErrorCallback, base::Unretained(this)));

  if (!db_.Open(db_path_)) {
    LOG(ERROR) << "Failed to open CriticalAction database: "
               << db_path_.value();
    return false;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  if (!meta_table_.Init(&db_, kCurrentVersion, kCompatibleVersion)) {
    return false;
  }

  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersion) {
    LOG(ERROR) << "CriticalAction database is too new.";
    return false;
  }

  if (!InitSchema()) {
    return false;
  }

  return transaction.Commit();
}

bool CriticalActionDatabase::InitSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_.Execute("CREATE TABLE IF NOT EXISTS CriticalActions ("
                   "  critical_action_id TEXT PRIMARY KEY NOT NULL,"
                   "  timestamp INTEGER NOT NULL,"
                   "  visit_id INTEGER,"
                   "  conversation_id TEXT,"
                   "  actor_task_id TEXT,"
                   "  action_type INTEGER NOT NULL,"
                   "  url TEXT,"
                   "  metadata TEXT"
                   ")")) {
    return false;
  }

  if (!db_.Execute("CREATE INDEX IF NOT EXISTS idx_criticalactions_visit_id ON "
                   "CriticalActions(visit_id)")) {
    return false;
  }

  if (!db_.Execute(
          "CREATE INDEX IF NOT EXISTS idx_criticalactions_timestamp ON "
          "CriticalActions(timestamp)")) {
    return false;
  }

  if (!db_.Execute(
          "CREATE INDEX IF NOT EXISTS idx_criticalactions_action_type ON "
          "CriticalActions(action_type)")) {
    return false;
  }

  if (!db_.Execute(
          "CREATE INDEX IF NOT EXISTS idx_criticalactions_conversation_id ON "
          "CriticalActions(conversation_id)")) {
    return false;
  }

  if (!db_.Execute("CREATE INDEX IF NOT EXISTS idx_criticalactions_url ON "
                   "CriticalActions(url)")) {
    return false;
  }

  if (!db_.Execute(
          "CREATE INDEX IF NOT EXISTS idx_criticalactions_actor_task_id ON "
          "CriticalActions(actor_task_id)")) {
    return false;
  }

  return true;
}

bool CriticalActionDatabase::AddCriticalAction(
    const CriticalActionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO CriticalActions (critical_action_id, timestamp, visit_id, "
      "conversation_id, actor_task_id, action_type, url, metadata) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

  statement.BindString(0, entry.critical_action_id);
  statement.BindTime(1, entry.timestamp);
  statement.BindInt64(2, entry.visit_id);
  statement.BindString(3, entry.conversation_id);
  statement.BindString(4, entry.actor_task_id);
  statement.BindInt(5, static_cast<int>(entry.action_type));
  statement.BindString(6, entry.url.spec());
  statement.BindString(7, entry.metadata);

  return statement.Run();
}

std::optional<CriticalActionEntry> CriticalActionDatabase::GetCriticalAction(
    std::string_view critical_action_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT critical_action_id, timestamp, visit_id, conversation_id, "
      "actor_task_id, action_type, url, metadata FROM CriticalActions "
      "WHERE critical_action_id = ?"));

  statement.BindString(0, critical_action_id);

  if (!statement.Step()) {
    return std::nullopt;
  }

  CriticalActionEntry entry;
  entry.critical_action_id = statement.ColumnString(0);
  entry.timestamp = statement.ColumnTime(1);
  entry.visit_id = statement.ColumnInt64(2);
  entry.conversation_id = statement.ColumnString(3);
  entry.actor_task_id = statement.ColumnString(4);
  entry.action_type = static_cast<ActionType>(statement.ColumnInt(5));
  entry.url = GURL(statement.ColumnString(6));
  entry.metadata = statement.ColumnString(7);

  return entry;
}

std::vector<CriticalActionEntry> CriticalActionDatabase::GetCriticalActions(
    const CriticalActionQueryOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<CriticalActionEntry> entries;

  std::vector<std::string> conditions;
  std::string sql_query =
      "SELECT critical_action_id, timestamp, visit_id, conversation_id, "
      "actor_task_id, action_type, url, metadata FROM CriticalActions";

  if (options.begin_time.has_value()) {
    conditions.push_back("timestamp >= ?");
  }
  if (options.end_time.has_value()) {
    conditions.push_back("timestamp < ?");
  }
  if (!options.action_types.empty()) {
    std::string condition = "action_type IN (";
    for (size_t i = 0; i < options.action_types.size(); ++i) {
      if (i > 0) {
        condition += ", ";
      }
      condition += "?";
    }
    condition += ")";
    conditions.push_back(condition);
  }
  if (options.conversation_id.has_value()) {
    conditions.push_back("conversation_id = ?");
  }
  if (options.actor_task_id.has_value()) {
    conditions.push_back("actor_task_id = ?");
  }

  if (!conditions.empty()) {
    sql_query += " WHERE ";
    for (size_t i = 0; i < conditions.size(); ++i) {
      if (i > 0) {
        sql_query += " AND ";
      }
      sql_query += conditions[i];
    }
  }

  sql_query += " ORDER BY timestamp DESC";

  if (options.max_count.has_value()) {
    sql_query += " LIMIT ?";
  }

  sql::Statement statement(db_.GetUniqueStatement(sql_query));

  int bind_index = 0;
  if (options.begin_time.has_value()) {
    statement.BindTime(bind_index++, *options.begin_time);
  }
  if (options.end_time.has_value()) {
    statement.BindTime(bind_index++, *options.end_time);
  }
  if (!options.action_types.empty()) {
    for (ActionType type : options.action_types) {
      statement.BindInt(bind_index++, static_cast<int>(type));
    }
  }
  if (options.conversation_id.has_value()) {
    statement.BindString(bind_index++, *options.conversation_id);
  }
  if (options.actor_task_id.has_value()) {
    statement.BindString(bind_index++, *options.actor_task_id);
  }
  if (options.max_count.has_value()) {
    statement.BindInt64(bind_index++, *options.max_count);
  }

  while (statement.Step()) {
    CriticalActionEntry entry;
    entry.critical_action_id = statement.ColumnString(0);
    entry.timestamp = statement.ColumnTime(1);
    entry.visit_id = statement.ColumnInt64(2);
    entry.conversation_id = statement.ColumnString(3);
    entry.actor_task_id = statement.ColumnString(4);
    entry.action_type = static_cast<ActionType>(statement.ColumnInt(5));
    entry.url = GURL(statement.ColumnString(6));
    entry.metadata = statement.ColumnString(7);
    entries.push_back(std::move(entry));
  }

  return entries;
}

bool CriticalActionDatabase::DeleteCriticalAction(
    std::string_view critical_action_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM CriticalActions WHERE critical_action_id = ?"));

  statement.BindString(0, critical_action_id);
  return statement.Run();
}

bool CriticalActionDatabase::DeleteCriticalActionsInTimeRange(
    base::Time start_time,
    base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM CriticalActions WHERE timestamp >= ? AND timestamp < ?"));

  statement.BindTime(0, start_time);
  statement.BindTime(1, end_time);
  return statement.Run();
}

bool CriticalActionDatabase::DeleteCriticalActionsByVisitIds(
    const std::vector<int64_t>& visit_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (visit_ids.empty()) {
    return true;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM CriticalActions WHERE visit_id = ?"));

  for (int64_t visit_id : visit_ids) {
    statement.Reset(true);
    statement.BindInt64(0, visit_id);
    if (!statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

void CriticalActionDatabase::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_.Close();
}

void CriticalActionDatabase::DatabaseErrorCallback(int extended_error,
                                                   sql::Statement* statement) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sql::IsErrorCatastrophic(extended_error)) {
    db_.RazeAndPoison();
  } else if (!sql::Database::IsExpectedSqliteError(extended_error)) {
    LOG(ERROR) << "CriticalAction Database error: " << extended_error;
  }
}

}  // namespace critical_actions
