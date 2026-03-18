// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_FAKE_DOM_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_FAKE_DOM_STORAGE_DATABASE_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"

namespace storage {

// A fake DomStorageDatabase for use in tests. All methods return OK / empty
// results by default. Use setters to configure specific return values.
// Like production instances, this is base::SequenceBound.
class FakeDomStorageDatabase : public DomStorageDatabase {
 public:
  // `open_status` is returned by Open(). Passed at construction time because
  // DomStorageDatabaseFactory::Create() is immediately followed by an Open()
  // call before the test can configure the instance.
  explicit FakeDomStorageDatabase(DbStatus open_status);
  ~FakeDomStorageDatabase() override;

  // Setters for configuring return values of interface methods.
  void SetReadAllMetadataResult(StatusOr<Metadata> result);
  void SetUpdateMapsStatus(DbStatus status);

  // DomStorageDatabase:
  DbStatus Open(const base::FilePath& database_path,
                const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
                    memory_dump_id) override;
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
  DbStatus CleanUpStaleData() override;

  // Test-only DomStorageDatabase methods. Stubbed for now.
  // TODO(crbug.com/377242771): Update tests using these to rely on this fake
  // implementation. Then remove these methods.
  DbStatus PutVersionForTesting(int64_t version) override;
  void MakeAllCommitsFailForTesting() override;
  void SetDestructionCallbackForTesting(base::OnceClosure callback) override;

 private:
  DbStatus open_status_;
  StatusOr<Metadata> read_all_metadata_result_;
  DbStatus update_maps_status_ = DbStatus::OK();
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_FAKE_DOM_STORAGE_DATABASE_H_
