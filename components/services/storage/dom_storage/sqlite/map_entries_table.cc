// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/map_entries_table.h"

#include "components/services/storage/dom_storage/sqlite/sqlite_database_utils.h"
#include "sql/database.h"

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
MapEntriesTable::GetMapKeyValues(
    const DomStorageDatabase::MapLocator& map_locator) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return base::unexpected(DbStatus::NotSupported(""));
}

DbStatus MapEntriesTable::UpdateMaps(
    std::vector<DomStorageDatabase::MapBatchUpdate> map_updates) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus MapEntriesTable::DeleteMaps(
    std::vector<DomStorageDatabase::MapLocator> maps_to_delete) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

}  // namespace storage
