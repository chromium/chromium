// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/dom_storage_sqlite.h"

namespace storage {

DomStorageSqlite::DomStorageSqlite(PassKey) {}

DomStorageSqlite::~DomStorageSqlite() = default;

DbStatus DomStorageSqlite::Open(
    PassKey,
    const base::FilePath& directory,
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DomStorageDatabaseLevelDB& DomStorageSqlite::GetLevelDB() {
  NOTREACHED();
}

StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>
DomStorageSqlite::ReadMapKeyValues(MapLocator map_locator) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return base::unexpected(DbStatus::NotSupported(""));
}

DbStatus DomStorageSqlite::UpdateMaps(std::vector<MapBatchUpdate> map_updates) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus DomStorageSqlite::CloneMap(MapLocator source_map,
                                    MapLocator target_map) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

StatusOr<DomStorageDatabase::Metadata> DomStorageSqlite::ReadAllMetadata() {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return base::unexpected(DbStatus::NotSupported(""));
}

DbStatus DomStorageSqlite::PutMetadata(Metadata metadata) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus DomStorageSqlite::DeleteStorageKeysFromSession(
    std::string session_id,
    std::vector<blink::StorageKey> metadata_to_delete,
    std::vector<MapLocator> maps_to_delete) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus DomStorageSqlite::DeleteSessions(
    std::vector<std::string> session_ids,
    std::vector<MapLocator> maps_to_delete) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus DomStorageSqlite::PurgeOrigins(std::set<url::Origin> origins) {
  return DbStatus::NotSupported("");
}

DbStatus DomStorageSqlite::RewriteDB() {
  // SQLite does not need to rewrite its database to fully erase deleted data.
  return DbStatus::OK();
}

void DomStorageSqlite::MakeAllCommitsFailForTesting() {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
}

void DomStorageSqlite::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
}

}  // namespace storage
