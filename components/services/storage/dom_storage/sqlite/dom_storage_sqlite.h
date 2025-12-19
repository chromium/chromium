// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_DOM_STORAGE_SQLITE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_DOM_STORAGE_SQLITE_H_

#include "components/services/storage/dom_storage/dom_storage_database.h"

namespace storage {
class DomStorageDatabaseLevelDB;

// TODO(crbug.com/377242771): Work in progress. Persists local/session
// storage metadata and key/value pairs using SQLite.
class DomStorageSqlite : public DomStorageDatabase {
 private:
  using PassKey = base::PassKey<DomStorageDatabaseFactory>;

 public:
  // Use `DomStorageDatabaseFactory::Open()` to construct a
  // base::SequenceBound<DomStorageDatabase>.
  explicit DomStorageSqlite(PassKey);
  ~DomStorageSqlite() override;

  DomStorageSqlite(const DomStorageSqlite&) = delete;
  DomStorageSqlite& operator=(const DomStorageSqlite&) = delete;

  DbStatus Open(PassKey,
                const base::FilePath& directory,
                const std::string& name,
                const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
                    memory_dump_id);

  // The `DomStorageDatabase` interface:
  DomStorageDatabaseLevelDB& GetLevelDB() override;
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

 private:
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_DOM_STORAGE_SQLITE_H_
