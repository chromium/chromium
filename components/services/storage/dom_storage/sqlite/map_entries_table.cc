// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/map_entries_table.h"

#include "components/services/storage/dom_storage/sqlite/sqlite_database_macros.h"
#include "components/services/storage/dom_storage/sqlite/sqlite_database_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace storage {

DbStatus MapEntriesTable::CreateSchema(sql::Database& database) {
  const char kCreateMapEntriesTable[] =
      // clang-format off
      "CREATE TABLE map_entries("
        "map_id INTEGER NOT NULL,"
        "key BLOB NOT NULL,"
        "value BLOB NOT NULL,"
        "PRIMARY KEY(map_id, key)"
      ") WITHOUT ROWID;";
  // clang-format on

  if (!database.Execute(kCreateMapEntriesTable)) {
    return storage::FromSqliteCode(database);
  }
  return DbStatus::OK();
}

MapEntriesTable::MapEntriesTable(sql::Database& database)
    : database_(&database) {}

MapEntriesTable::~MapEntriesTable() = default;

StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>
MapEntriesTable::GetMapKeyValues(int64_t map_id) {
  constexpr const char kSelectMapEntries[] =
      "SELECT key, value FROM map_entries WHERE map_id = ?";

  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kSelectMapEntries));
  statement.BindInt64(0, map_id);

  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> entries;
  while (statement.Step()) {
    entries.emplace(/*key=*/statement.ColumnBlobAsVector(0),
                    /*value=*/statement.ColumnBlobAsVector(1));
  }
  RETURN_UNEXPECTED_ON_ERROR(statement.Succeeded());
  return entries;
}

DbStatus MapEntriesTable::UpdateMap(
    DomStorageDatabase::MapBatchUpdate map_update) {
  CHECK(database_->HasActiveTransactions());
  int64_t map_id = map_update.map_locator.map_id().value();

  // If `clear_all_first` is set, delete all entries for this map.
  if (map_update.clear_all_first) {
    DB_RETURN_IF_ERROR(DeleteMap(map_id));
  } else {
    // Delete the specified keys.
    constexpr const char kDeleteEntry[] =
        "DELETE FROM map_entries WHERE map_id = ? AND key = ?";

    sql::Statement delete_entry_statement(
        database_->GetCachedStatement(SQL_FROM_HERE, kDeleteEntry));
    delete_entry_statement.BindInt64(0, map_id);

    for (auto& key : map_update.keys_to_delete) {
      delete_entry_statement.BindBlob(1, std::move(key));
      RETURN_STATUS_ON_ERROR(delete_entry_statement.Run());
      delete_entry_statement.Reset(/*clear_bound_vars=*/false);
    }
  }

  // Insert or replace entries.
  constexpr const char kInsertOrReplaceEntry[] =
      "INSERT OR REPLACE INTO map_entries(map_id, key, value) VALUES( ?, ?, ?)";

  sql::Statement insert_statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kInsertOrReplaceEntry));
  insert_statement.BindInt64(0, map_id);

  for (const auto& entry : map_update.entries_to_add) {
    insert_statement.BindBlob(1, std::move(entry.key));
    insert_statement.BindBlob(2, std::move(entry.value));
    RETURN_STATUS_ON_ERROR(insert_statement.Run());
    insert_statement.Reset(/*clear_bound_vars=*/false);
  }
  return DbStatus::OK();
}

DbStatus MapEntriesTable::DeleteMap(int64_t map_id) {
  CHECK(database_->HasActiveTransactions());

  constexpr const char kDeleteAllEntries[] =
      "DELETE FROM map_entries WHERE map_id = ?";

  sql::Statement delete_all_statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllEntries));
  delete_all_statement.BindInt64(0, map_id);

  RETURN_STATUS_ON_ERROR(delete_all_statement.Run());
  return DbStatus::OK();
}

}  // namespace storage
