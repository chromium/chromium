// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_SESSION_STORAGE_SQLITE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_SESSION_STORAGE_SQLITE_H_

#include "base/trace_event/memory_allocator_dump_guid.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"

namespace sql {
class Database;
class MetaTable;
}  // namespace sql

namespace storage {
class MapEntriesTable;

// Session storage creates two tables in the database. It also stores the next
// map ID in the `meta_table_`. For example, the `session_metadata` table
// contains rows like:
//
// ----------------------------------------------------------------------------
// | session_id (TEXT)                    | storage_key (BLOB) | map_id (INT) |
// ----------------------------------------------------------------------------
// | dabc53e1_8291_4de5_824f_dab8aa69c846 | https://a.test/    | 1            |
// | dabc53e1_8291_4de5_824f_dab8aa69c846 | https://b.test/    | 2            |
// | ce8c7dc5_73b4_4320_a506_ce1f4fd3356f | https://a.test/    | 3            |
// | 36356e0b_1627_4492_a474_db76a8996bed | https://a.test/    | 1            |
//
// The table above contains 3 different sessions and 3 different maps:
//
// The first session (dabc53e1) contains two maps: one for https://a.test and
// one for https://b.test.
//
// The second session (ce8c7dc5) contains a different map for https://a.test.
//
// The third session (36356e0b) contains a clone of the first session's map for
// https://a.test/.
//
// The `map_id` column references the `map_id` column in the
// `map_entries_table_`, which is used to read and write a map's key/value
// pairs.
//
// Also, in this example, the `meta_table_` contains the key/value pair:
//
// "next_map_id" = 4 (where 4 is an integer)
class SessionStorageSqlite : public DomStorageDatabase {
 private:
  using PassKey = base::PassKey<DomStorageDatabaseFactory>;

 public:
  // Use `DomStorageDatabaseFactory::Open()` to construct a
  // base::SequenceBound<DomStorageDatabase>.
  explicit SessionStorageSqlite(PassKey);
  ~SessionStorageSqlite() override;

  SessionStorageSqlite(const SessionStorageSqlite&) = delete;
  SessionStorageSqlite& operator=(const SessionStorageSqlite&) = delete;

  DbStatus Open(PassKey,
                const base::FilePath& database_path,
                const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
                    memory_dump_id);

  // The `DomStorageDatabase` interface:
  StatusOr<std::map<Key, Value>> ReadMapKeyValues(
      MapLocator map_locator) override;
  DbStatus UpdateMaps(std::vector<MapBatchUpdate> map_updates) override;
  DbStatus CloneMap(MapLocator source_map, MapLocator target_map) override;
  StatusOr<Metadata> ReadAllMetadata() override;
  DbStatus PutMetadata(Metadata metadata) override;
  DbStatus DeleteStorageKeysFromSession(
      std::string session_id,
      std::vector<blink::StorageKey> metadata_to_delete,
      std::vector<MapLocator> maps_to_delete) override;
  DbStatus DeleteSessions(std::vector<std::string> session_ids,
                          std::vector<MapLocator> maps_to_delete) override;
  DbStatus PurgeOrigins(std::set<url::Origin> origins) override;
  DbStatus RewriteDB() override;
  void MakeAllCommitsFailForTesting() override;
  void SetDestructionCallbackForTesting(base::OnceClosure callback) override;
  DbStatus PutVersionForTesting(int64_t version) override;

 private:
  // Reads the `next_map_id` value from the meta table. Returns 0 if no value
  // has been stored yet.
  int64_t ReadNextMapId() const;

  // Reads all rows from the `session_metadata` table and returns them as a
  // vector of `MapMetadata`. Rows with the same `map_id` (cloned maps) are
  // merged into a single `MapMetadata` entry with multiple session IDs in its
  // `MapLocator`.
  StatusOr<std::vector<DomStorageDatabase::MapMetadata>> ReadAllMapMetadata()
      const;

  // `Open()` creates `database_`, `meta_table_` and `map_entries_table_`.
  std::unique_ptr<sql::Database> database_;
  std::unique_ptr<sql::MetaTable> meta_table_;
  std::unique_ptr<MapEntriesTable> map_entries_table_;

  // Simulates I/O failure in `PutMetadata()` and `UpdateMaps()` by force
  // returning an IOError. Set to true by `MakeAllCommitsFailForTesting()`.
  bool should_fail_commits_for_testing_ = false;

  base::OnceClosure destruction_callback_for_testing_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_SESSION_STORAGE_SQLITE_H_
