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
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

// Holds the metadata information for a session storage database. This includes
// logic for parsing and saving database content.
class SessionStorageMetadata {
 public:
  // LevelDB supports one schema version for session storage without migration.
  static constexpr const int64_t kLevelDbSchemaVersion = 1;

  static constexpr const int64_t kInvalidMapId = -1;

  static constexpr const uint8_t kLevelDbSchemaVersionKeyBytes[] = {
      'v', 'e', 'r', 's', 'i', 'o', 'n'};

  static constexpr const uint8_t kNamespacePrefixBytes[] = {
      'n', 'a', 'm', 'e', 's', 'p', 'a', 'c', 'e', '-'};

  // This is "next-map-id" (without the quotes).
  static constexpr const uint8_t kNextMapIdKeyBytes[] = {
      'n', 'e', 'x', 't', '-', 'm', 'a', 'p', '-', 'i', 'd'};

  // Represents a map which can be shared by multiple areas.
  // The |DeleteNamespace| and |DeleteArea| methods can destroy any MapData
  // objects who are no longer referenced by another namespace.
  // Maps (and thus MapData objects) can only be shared for the same StorageKey.
  class MapData : public base::RefCounted<MapData> {
   public:
    explicit MapData(int64_t map_number, blink::StorageKey storage_key);

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
    std::vector<uint8_t> key_prefix_;
    blink::StorageKey storage_key_;
    int reference_count_ = 0;
  };

  using NamespaceStorageKeyMap =
      std::map<std::string,
               std::map<blink::StorageKey, scoped_refptr<MapData>>>;
  using NamespaceEntry = NamespaceStorageKeyMap::iterator;

  SessionStorageMetadata();
  ~SessionStorageMetadata();

  // Initializes a new test database, which saves the database version, clears
  // the metadata, and returns the operations needed to save to disk.
  std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>
  SetupNewDatabaseForTesting();

  // Parses the database version number from the bytes that were stored on
  // disk. LevelDB session storage persists the version number as text
  // characters. For example, `version_text_bytes` might be `{ '6', '4', '5' }
  // for version number 645. Returns false when `version_text_bytes` are not a
  // number.
  [[nodiscard]] static bool ParseDatabaseVersion(
      std::vector<uint8_t> version_text_bytes,
      int64_t* parsed_version);

  // Parses all namespaces and maps, and stores all metadata locally. This
  // invalidates all NamespaceEntry and MapData objects. If there is a parsing
  // error, the namespaces will be cleared. This call is not necessary on new
  // databases.
  bool ParseNamespaces(std::vector<DomStorageDatabase::KeyValuePair> values);

  // Parses the next map id from the given bytes. If that fails, then it uses
  // the next available id from parsing the namespaces. This call is not
  // necessary on new databases.
  void ParseNextMapId(const std::vector<uint8_t>& map_id);

  // Creates new map data for the given namespace-StorageKey area. If the area
  // entry exists, then it will decrement the refcount of the old map. Tasks
  // appended to |*save_tasks| if run will save the new or modified area entry
  // to disk, as well as saving the next available map id.
  //
  // NOTE: It is invalid to call this method for an area that has a map with
  // only one reference.
  scoped_refptr<MapData> RegisterNewMap(
      NamespaceEntry namespace_entry,
      const blink::StorageKey& storage_key,
      std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>* save_tasks);

  // Registers an StorageKey-map in the |destination_namespace| from every
  // StorageKey-map in the |source_namespace|. The |destination_namespace| must
  // have no StorageKey-maps. All maps in the destination namespace are the same
  // maps as the source namespace. All database operations to save the namespace
  // StorageKey metadata are put in |save_tasks|.
  void RegisterShallowClonedNamespace(
      NamespaceEntry source_namespace,
      NamespaceEntry destination_namespace,
      std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>* save_tasks);

  // Deletes the given namespace and any maps that no longer have any
  // references. This will invalidate all NamespaceEntry objects for the
  // |namespace_id|, and can invalidate any MapData objects whose reference
  // count hits zero. Appends operations to |*save_tasks| which will commit the
  // deletions to disk if run.
  void DeleteNamespace(
      const std::string& namespace_id,
      std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>* save_tasks);

  // This returns a BatchDatabaseTask to remove the metadata entry for this
  // namespace-StorageKey area. If the map at this entry isn't referenced by any
  // other area (refcount hits 0), then the task will also delete that map on
  // disk and invalidate that MapData.
  void DeleteArea(
      const std::string& namespace_id,
      const blink::StorageKey& storage_key,
      std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>* save_tasks);

  NamespaceEntry GetOrCreateNamespaceEntry(const std::string& namespace_id);

  const NamespaceStorageKeyMap& namespace_storage_key_map() const {
    return namespace_storage_key_map_;
  }

  int64_t NextMapId() const { return next_map_id_; }

  static std::vector<uint8_t> LatestDatabaseVersionAsVector();

 private:
  static std::vector<uint8_t> GetNamespacePrefix(
      const std::string& namespace_id);
  static std::vector<uint8_t> GetAreaKey(const std::string& namespace_id,
                                         const blink::StorageKey& storage_key);
  static std::vector<uint8_t> GetMapPrefix(int64_t map_number);
  static std::vector<uint8_t> GetMapPrefix(
      const std::vector<uint8_t>& map_number_as_bytes);

  int64_t next_map_id_ = kInvalidMapId;
  int64_t next_map_id_from_namespaces_ = 0;

  NamespaceStorageKeyMap namespace_storage_key_map_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_METADATA_H_
