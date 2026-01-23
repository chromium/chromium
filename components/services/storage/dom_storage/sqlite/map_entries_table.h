// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_MAP_ENTRIES_TABLE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_MAP_ENTRIES_TABLE_H_

#include <map>
#include <vector>

#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "storage/common/database/db_status.h"

namespace sql {
class Database;
}  // namespace sql

namespace storage {

// Creates, reads and writes the `map_entries` table, which persists key/value
// pairs for multiple maps using the following columns:
//
// ------------------------------------------------
// | map_id (INTEGER) | key (BLOB) | value (BLOB) |
// ------------------------------------------------
// | 1                | S          | T            |
// | 2                | S          | U            |
// | 2                | V          | W            |
// | 3                | X          | Y            |
//
// JavaScript provides the keys and values. Session storage and local storage
// both use the `MapEntriesTable`. See `session_storage_sqlite.h`
// and `local_storage_sqlite.h` for more detail.
class MapEntriesTable {
 public:
  // Initializes the `map_entries` table for a brand new database.
  static DbStatus CreateSchema(sql::Database& database);

  explicit MapEntriesTable(sql::Database& database);
  ~MapEntriesTable();

  // Read all of a map's key/value pairs.
  StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>
  GetMapKeyValues(const DomStorageDatabase::MapLocator& map_locator);

  // Persist all `map_updates`. Each update adds, modifies and/or deletes
  // key/value pairs in a map.
  DbStatus UpdateMaps(
      std::vector<DomStorageDatabase::MapBatchUpdate> map_updates);

  // Delete all of the key/value pairs for each map in `maps_to_delete`.
  DbStatus DeleteMaps(
      std::vector<DomStorageDatabase::MapLocator> maps_to_delete);

 private:
  raw_ptr<sql::Database> database_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_MAP_ENTRIES_TABLE_H_
