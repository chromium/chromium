// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_database.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "sql/error_delegate_util.h"
#include "sql/sqlite_result_code.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace critical_actions {

namespace {

// Batch size of 500 amortizes query overhead while staying safely below
// SQLite's parameter limits (SQLITE_MAX_VARIABLE_NUMBER) and minimizing
// parser memory.
constexpr size_t kMaxBatchSize = 500;
constexpr int kCurrentVersion = 2;
constexpr int kCompatibleVersion = 1;

constexpr std::string_view kTableNames[] = {
    "CriticalActionConversations",
    "CriticalActionVisits",
    "CriticalActionEntries",
};

sql::DatabaseOptions GetDatabaseOptions() {
  sql::DatabaseOptions options;
  options.set_page_size(4096);
  options.set_cache_size(32);
  return options;
}

std::string BuildDeleteQuery(std::string_view table_name) {
  return base::StrCat(
      {"DELETE FROM ", table_name, " WHERE critical_action_id = ?"});
}

std::string BuildDeleteInTimeRangeQuery(std::string_view table_name) {
  if (table_name == "CriticalActionEntries") {
    return "DELETE FROM CriticalActionEntries WHERE timestamp >= ? AND "
           "timestamp < ?";
  }
  return base::StrCat({
      "DELETE FROM ",
      table_name,
      " WHERE critical_action_id IN ("
      "  SELECT critical_action_id FROM CriticalActionEntries WHERE timestamp "
      ">= ? AND timestamp < ?)",
  });
}

std::string BuildDeleteByVisitIdsQuery(std::string_view table_name,
                                       std::string_view placeholders) {
  if (table_name == "CriticalActionVisits") {
    return base::StrCat({"DELETE FROM CriticalActionVisits WHERE visit_id IN (",
                         placeholders, ")"});
  }
  return base::StrCat({
      "DELETE FROM ",
      table_name,
      " WHERE critical_action_id IN ("
      "  SELECT critical_action_id FROM CriticalActionVisits WHERE visit_id "
      "IN (",
      placeholders,
      "))",
  });
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

  if (!MigrateToCurrentVersion()) {
    return false;
  }

  if (!InitSchema()) {
    return false;
  }

  return transaction.Commit();
}

bool CriticalActionDatabase::InitSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_.Execute("CREATE TABLE IF NOT EXISTS CriticalActionEntries ("
                   "  critical_action_id TEXT PRIMARY KEY NOT NULL,"
                   "  timestamp INTEGER NOT NULL,"
                   "  actor_task_id TEXT,"
                   "  action_type INTEGER NOT NULL,"
                   "  url TEXT,"
                   "  metadata TEXT"
                   ")")) {
    return false;
  }

  if (!db_.Execute("CREATE INDEX IF NOT EXISTS idx_entries_timestamp ON "
                   "CriticalActionEntries(timestamp)")) {
    return false;
  }

  if (!db_.Execute("CREATE INDEX IF NOT EXISTS idx_entries_action_type ON "
                   "CriticalActionEntries(action_type)")) {
    return false;
  }

  if (!db_.Execute("CREATE INDEX IF NOT EXISTS idx_entries_actor_task_id ON "
                   "CriticalActionEntries(actor_task_id)")) {
    return false;
  }

  if (!db_.Execute("CREATE INDEX IF NOT EXISTS idx_entries_url ON "
                   "CriticalActionEntries(url)")) {
    return false;
  }

  if (!db_.Execute("CREATE TABLE IF NOT EXISTS CriticalActionVisits ("
                   "  critical_action_id TEXT PRIMARY KEY NOT NULL,"
                   "  visit_id INTEGER NOT NULL"
                   ")")) {
    return false;
  }

  if (!db_.Execute("CREATE INDEX IF NOT EXISTS idx_visits_visit_id ON "
                   "CriticalActionVisits(visit_id)")) {
    return false;
  }

  if (!db_.Execute("CREATE TABLE IF NOT EXISTS CriticalActionConversations ("
                   "  critical_action_id TEXT PRIMARY KEY NOT NULL,"
                   "  conversation_id TEXT NOT NULL"
                   ")")) {
    return false;
  }

  if (!db_.Execute(
          "CREATE INDEX IF NOT EXISTS idx_conversations_conversation_id ON "
          "CriticalActionConversations(conversation_id)")) {
    return false;
  }

  return true;
}

bool CriticalActionDatabase::MigrateToCurrentVersion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (int next_version = meta_table_.GetVersionNumber() + 1;
       next_version <= kCurrentVersion; ++next_version) {
    if (!MigrateToVersion(next_version)) {
      LOG(ERROR) << "Failed to migrate to version " << next_version;
      return false;
    }
    if (!meta_table_.SetVersionNumber(next_version)) {
      return false;
    }
  }

  return true;
}

bool CriticalActionDatabase::MigrateToVersion(int version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (version) {
    case 2:
      return MigrateFromV1ToV2();
    default:
      return true;
  }
}

bool CriticalActionDatabase::MigrateFromV1ToV2() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  if (!InitSchema()) {
    return false;
  }

  if (db_.DoesTableExist("CriticalActions")) {
    if (!db_.Execute(
            "INSERT INTO CriticalActionEntries "
            "(critical_action_id, timestamp, action_type, url, metadata, "
            "actor_task_id) "
            "SELECT critical_action_id, timestamp, action_type, url, metadata, "
            "actor_task_id FROM CriticalActions")) {
      return false;
    }

    if (!db_.Execute(
            "INSERT INTO CriticalActionVisits (critical_action_id, visit_id) "
            "SELECT critical_action_id, visit_id FROM CriticalActions "
            "WHERE visit_id IS NOT NULL AND visit_id > 0")) {
      return false;
    }

    if (!db_.Execute(
            "INSERT INTO CriticalActionConversations (critical_action_id, "
            "conversation_id) "
            "SELECT critical_action_id, conversation_id FROM CriticalActions "
            "WHERE conversation_id IS NOT NULL AND conversation_id != ''")) {
      return false;
    }

    if (!db_.Execute("DROP TABLE CriticalActions")) {
      return false;
    }
  }

  return transaction.Commit();
}

bool CriticalActionDatabase::AddCriticalAction(
    const CriticalActionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement stmt_entry(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO CriticalActionEntries (critical_action_id, timestamp, "
      "action_type, url, metadata, actor_task_id) "
      "VALUES (?, ?, ?, ?, ?, ?)"));
  stmt_entry.BindString(0, entry.critical_action_id);
  stmt_entry.BindTime(1, entry.timestamp);
  stmt_entry.BindInt(2, static_cast<int>(entry.action_type));
  stmt_entry.BindString(3, entry.url.spec());
  stmt_entry.BindString(4, entry.metadata);
  stmt_entry.BindString(5, entry.actor_task_id);
  if (!stmt_entry.Run()) {
    return false;
  }

  if (entry.visit_id > 0) {
    sql::Statement stmt_visit(db_.GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT INTO CriticalActionVisits (critical_action_id, "
        "visit_id) VALUES (?, ?)"));
    stmt_visit.BindString(0, entry.critical_action_id);
    stmt_visit.BindInt64(1, entry.visit_id);
    if (!stmt_visit.Run()) {
      return false;
    }
  }

  if (!entry.conversation_id.empty()) {
    sql::Statement stmt_conv(db_.GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT INTO CriticalActionConversations "
        "(critical_action_id, conversation_id) VALUES (?, ?)"));
    stmt_conv.BindString(0, entry.critical_action_id);
    stmt_conv.BindString(1, entry.conversation_id);
    if (!stmt_conv.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

std::optional<CriticalActionEntry> CriticalActionDatabase::GetCriticalAction(
    std::string_view critical_action_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT e.critical_action_id, e.timestamp, v.visit_id, "
      "c.conversation_id, "
      "       e.actor_task_id, e.action_type, e.url, e.metadata "
      "FROM CriticalActionEntries e "
      "LEFT JOIN CriticalActionVisits v ON e.critical_action_id = "
      "v.critical_action_id "
      "LEFT JOIN CriticalActionConversations c ON e.critical_action_id = "
      "c.critical_action_id "
      "WHERE e.critical_action_id = ?"));

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
      "SELECT e.critical_action_id, e.timestamp, v.visit_id, "
      "c.conversation_id, "
      "       e.actor_task_id, e.action_type, e.url, e.metadata "
      "FROM CriticalActionEntries e "
      "LEFT JOIN CriticalActionVisits v ON e.critical_action_id = "
      "v.critical_action_id "
      "LEFT JOIN CriticalActionConversations c ON e.critical_action_id = "
      "c.critical_action_id";
  if (options.begin_time.has_value()) {
    conditions.push_back("e.timestamp >= ?");
  }
  if (options.end_time.has_value()) {
    conditions.push_back("e.timestamp < ?");
  }
  if (!options.action_types.empty()) {
    std::string condition = "e.action_type IN (";
    for (size_t i = 0; i < options.action_types.size(); ++i) {
      if (i > 0) {
        condition += ", ";
      }
      condition += "?";
    }
    condition += ")";
    conditions.push_back(condition);
  }

  // TODO(b/543797083): Critical actions are currently stored locally and
  // not synced between devices. As a result, visits that occurred on
  // other devices will not have matching critical actions in the local
  // database.
  if (!options.visit_ids.empty()) {
    std::string condition = "v.visit_id IN (";
    for (size_t i = 0; i < options.visit_ids.size(); ++i) {
      if (i > 0) {
        condition += ", ";
      }
      condition += "?";
    }
    condition += ")";
    conditions.push_back(condition);
  }
  if (options.conversation_id.has_value()) {
    conditions.push_back("c.conversation_id = ?");
  }
  if (options.actor_task_id.has_value()) {
    conditions.push_back("e.actor_task_id = ?");
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

  sql_query += " ORDER BY e.timestamp DESC";

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
  if (!options.visit_ids.empty()) {
    for (int64_t visit_id : options.visit_ids) {
      statement.BindInt64(bind_index++, visit_id);
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

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  for (const auto& table_name : kTableNames) {
    sql::Statement stmt(db_.GetUniqueStatement(BuildDeleteQuery(table_name)));
    stmt.BindString(0, critical_action_id);
    if (!stmt.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

bool CriticalActionDatabase::DeleteCriticalActionsInTimeRange(
    base::Time start_time,
    base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  for (const auto& table_name : kTableNames) {
    sql::Statement stmt(
        db_.GetUniqueStatement(BuildDeleteInTimeRangeQuery(table_name)));
    stmt.BindTime(0, start_time);
    stmt.BindTime(1, end_time);
    if (!stmt.Run()) {
      return false;
    }
  }

  return transaction.Commit();
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

  // Table deletion order matters, we must delete from the
  // CriticalActionVisits table last to avoid foreign key constraint
  // violations.
  static constexpr std::string_view kDeleteByVisitIdsTableNames[] = {
      "CriticalActionConversations",
      "CriticalActionEntries",
      "CriticalActionVisits",
  };

  for (size_t batch_start_index = 0; batch_start_index < visit_ids.size();
       batch_start_index += kMaxBatchSize) {
    const size_t batch_size =
        std::min(kMaxBatchSize, visit_ids.size() - batch_start_index);

    std::string placeholders;
    placeholders.reserve(batch_size * 2);
    for (size_t offset = 0; offset < batch_size; ++offset) {
      if (offset > 0) {
        placeholders += ",";
      }
      placeholders += "?";
    }

    for (const auto& table_name : kDeleteByVisitIdsTableNames) {
      sql::Statement stmt(db_.GetUniqueStatement(
          BuildDeleteByVisitIdsQuery(table_name, placeholders)));
      for (size_t offset = 0; offset < batch_size; ++offset) {
        stmt.BindInt64(offset, visit_ids[batch_start_index + offset]);
      }
      if (!stmt.Run()) {
        return false;
      }
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
  sql::UmaHistogramSqliteResult("CriticalActions.Database.SqliteError",
                                extended_error);
  if (sql::IsErrorCatastrophic(extended_error)) {
    db_.RazeAndPoison();
  } else if (!sql::Database::IsExpectedSqliteError(extended_error)) {
    LOG(ERROR) << "CriticalAction Database error: " << extended_error;
  }
}

}  // namespace critical_actions
