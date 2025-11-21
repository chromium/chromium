// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_METADATA_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_METADATA_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {
class DomStorageBatchOperationLevelDB;
class DomStorageDatabaseLevelDB;

// Holds the metadata information for a session storage database. This includes
// logic for parsing and saving database content.
class SessionStorageMetadata {
 public:
  // Represents a map which can be shared by multiple areas.
  // The |DeleteNamespace| and |DeleteArea| methods can destroy any MapData
  // objects who are no longer referenced by another namespace.
  // Maps (and thus MapData objects) can only be shared for the same StorageKey.
  class MapData : public base::RefCounted<MapData> {
   public:
    explicit MapData(int64_t map_number, blink::StorageKey storage_key);

    int64_t map_id() const { return map_id_; }
    const blink::StorageKey& storage_key() const { return storage_key_; }

    // The number of namespaces that reference this map.
    int ReferenceCount() const { return reference_count_; }

    // The key prefix for the map data (e.g. "map-2-").
    const std::vector<uint8_t>& KeyPrefix() const { return key_prefix_; }

    // The number of the map as bytes (e.g. "2").
    const std::vector<uint8_t>& MapNumberAsBytes() const {
      return number_as_bytes_;
    }

   private:
    friend class base::RefCounted<MapData>;
    friend class SessionStorageMetadata;
    ~MapData();

    void IncReferenceCount() { ++reference_count_; }
    void DecReferenceCount() { --reference_count_; }

    // The map number as bytes (e.g. "2"). These bytes are the string
    // representation of the map number.
    std::vector<uint8_t> number_as_bytes_;

    // `number_as_bytes_` as an `int64_t`.
    //
    // TODO(crbug.com/377242771): Remove `number_as_bytes_` after refactoring to
    // support a swappable backend for SQLite.
    int64_t map_id_;

    std::vector<uint8_t> key_prefix_;
    blink::StorageKey storage_key_;
    int reference_count_ = 0;
  };

  using NamespaceStorageKeyMap =
      std::map<std::string,
               std::map<blink::StorageKey, scoped_refptr<MapData>>>;
  using NamespaceEntry = NamespaceStorageKeyMap::iterator;

  // Populates the `DomStorageDatabase::Metadata::map_metadata` vector with the
  // `session_id`, storage key, and map ID for each `MapData` in `session`.
  static DomStorageDatabase::Metadata ToDomStorageMetadata(
      NamespaceEntry session);

  SessionStorageMetadata();
  ~SessionStorageMetadata();

  using BatchDatabaseTask =
      base::OnceCallback<void(DomStorageBatchOperationLevelDB&,
                              const DomStorageDatabaseLevelDB&)>;

  // Initializes a new test database, which clears the metadata and returns the
  // operations needed to save to disk.
  std::vector<BatchDatabaseTask> SetupNewDatabaseForTesting();

  // Populates `namespace_storage_key_map_` and sets `next_map_id_` using
  // `source`.
  void Initialize(DomStorageDatabase::Metadata source);

  // Creates new map data for the given namespace-StorageKey area. If the area
  // entry exists, then it will decrement the refcount of the old map.
  //
  // NOTE: It is invalid to call this method for an area that has a map with
  // only one reference.
  scoped_refptr<MapData> RegisterNewMap(NamespaceEntry namespace_entry,
                                        const blink::StorageKey& storage_key);

  // Registers an StorageKey-map in the |destination_namespace| from every
  // StorageKey-map in the |source_namespace|. The |destination_namespace| must
  // have no StorageKey-maps. All maps in the destination namespace are the same
  // maps as the source namespace.
  void RegisterShallowClonedNamespace(NamespaceEntry source_namespace,
                                      NamespaceEntry destination_namespace);

  // Deletes the given namespace and any maps that no longer have any
  // references. This will invalidate all NamespaceEntry objects for the
  // |namespace_id|, and can invalidate any MapData objects whose reference
  // count hits zero. Appends operations to |*save_tasks| which will commit the
  // deletions to disk if run.
  void DeleteNamespace(const std::string& namespace_id,
                       std::vector<BatchDatabaseTask>* save_tasks);

  // This returns a BatchDatabaseTask to remove the metadata entry for this
  // namespace-StorageKey area. If the map at this entry isn't referenced by any
  // other area (refcount hits 0), then the task will also delete that map on
  // disk and invalidate that MapData.
  void DeleteArea(const std::string& namespace_id,
                  const blink::StorageKey& storage_key,
                  std::vector<BatchDatabaseTask>* save_tasks);

  NamespaceEntry GetOrCreateNamespaceEntry(const std::string& namespace_id);

  const NamespaceStorageKeyMap& namespace_storage_key_map() const {
    return namespace_storage_key_map_;
  }

  int64_t NextMapId() const { return next_map_id_; }

 private:
  static std::vector<uint8_t> GetNamespacePrefix(
      const std::string& namespace_id);
  static std::vector<uint8_t> GetMapPrefix(int64_t map_number);
  static std::vector<uint8_t> GetMapPrefix(
      const std::vector<uint8_t>& map_number_as_bytes);

  int64_t next_map_id_ = 0;

  NamespaceStorageKeyMap namespace_storage_key_map_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_METADATA_H_
