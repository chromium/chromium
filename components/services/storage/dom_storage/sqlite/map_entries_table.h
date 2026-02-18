// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_MAP_ENTRIES_TABLE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_MAP_ENTRIES_TABLE_H_

#include <map>
#include <vector>

#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"

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
  GetMapKeyValues(int64_t map_id);

  // Persist `map_update`, which adds, modifies and/or deletes the map's
  // key/value pairs. The caller must begin a database transaction before
  // calling this function.
  DbStatus UpdateMap(DomStorageDatabase::MapBatchUpdate map_update);

  // Copies all key/value pairs from `source_map_id` to `target_map_id`. The
  // target map must be empty or non-existent; existing entries in the target
  // are not deleted first.
  DbStatus CloneMap(int64_t source_map_id, int64_t target_map_id);

  // Delete all of the key/value pairs in `map_id`. The
  // caller must begin a database transaction before calling this function.
  DbStatus DeleteMap(int64_t map_id);

 private:
  raw_ptr<sql::Database> database_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_MAP_ENTRIES_TABLE_H_
