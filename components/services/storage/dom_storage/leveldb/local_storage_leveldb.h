// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_LOCAL_STORAGE_LEVELDB_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_LOCAL_STORAGE_LEVELDB_H_

#include <memory>
#include <string>
#include <vector>

#include "base/byte_size.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/types/pass_key.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {
class DomStorageDatabaseLevelDB;

// The "METACCESS:" prefix.
inline constexpr const uint8_t kAccessMetaPrefix[] = {
    'M', 'E', 'T', 'A', 'A', 'C', 'C', 'E', 'S', 'S', ':'};

// The "META:" prefix.
inline constexpr const uint8_t kWriteMetaPrefix[] = {'M', 'E', 'T', 'A', ':'};

// The "META" prefix shared by both `kWriteMetaPrefix` and `kAccessMetaPrefix`.
inline constexpr const uint8_t kMetaPrefix[] = {'M', 'E', 'T', 'A'};

// The schema "VERSION" key.
inline constexpr const uint8_t kLocalStorageLevelDBVersionKey[] = {
    'V', 'E', 'R', 'S', 'I', 'O', 'N'};

// A component of the map key that separates the storage key from the script
// provided key. An example map key: "_<storage key>\x00<script key>"
const uint8_t kLocalStorageKeyMapSeparator = 0u;

// LevelDB supports one schema version for local storage without migration.
inline constexpr int64_t kLocalStorageLevelDBVersion = 1;

// Reads and writes entries in the local storage LevelDB database with the
// following schema:
//
//   key: "VERSION"
//   value: "1"
//
//   key: "METAACCESS:" + <StorageKey>
//   value: <LocalStorageAreaAccessMetaData serialized as a string>
//
//   key: "META:" + <StorageKey 'storage_key'>
//   value: <LocalStorageAreaWriteMetaData serialized as a string>
//
//   key: "_" + <StorageKey> + '\x00' + <script controlled key>
//   value: <script controlled value>
//
// Note: The StorageKeys are serialized as origins, not URLs, i.e. with no
// trailing slashes. For example, "https://abc.test".
class LocalStorageLevelDB : public DomStorageDatabase {
 private:
  using PassKey = base::PassKey<DomStorageDatabaseFactory>;

 public:
  // Use `DomStorageDatabaseFactory::Open()` to construct a
  // base::SequenceBound<DomStorageDatabase>.
  explicit LocalStorageLevelDB(PassKey);
  ~LocalStorageLevelDB() override;

  LocalStorageLevelDB(const LocalStorageLevelDB&) = delete;
  LocalStorageLevelDB& operator=(const LocalStorageLevelDB&) = delete;

  // Opens an on-disk or in-memory LevelDB and returns the result. To create an
  // in-memory database, provide an empty `directory`.
  DbStatus Open(PassKey,
                const base::FilePath& directory,
                const std::string& name,
                const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
                    memory_dump_id);

  // TODO(crbug.com/377242771): Make private after fully adopting the
  // `DomStorageDatabase` interface, which will make LevelDB and SQLite
  // swappable.
  //
  // Returns "METAACCESS:<serialized `storage_key`>".
  static Key CreateAccessMetaDataKey(const blink::StorageKey& storage_key);

  // TODO(crbug.com/377242771): Make private after fully adopting the
  // `DomStorageDatabase` interface, which will make LevelDB and SQLite
  // swappable.
  //
  // Returns "META:<serialized `storage_key`>".
  static Key CreateWriteMetaDataKey(const blink::StorageKey& storage_key);

  // TODO(crbug.com/377242771): Make private after fully adopting the
  // `DomStorageDatabase` interface, which will make LevelDB and SQLite
  // swappable.
  //
  // Return the the serialized bytes for the `LocalStorageAreaAccessMetaData`
  // protobuf with `last_accessed`.
  static Value CreateAccessMetaDataValue(base::Time last_accessed);

  // TODO(crbug.com/377242771): Make private after fully adopting the
  // `DomStorageDatabase` interface, which will make LevelDB and SQLite
  // swappable.
  //
  // Return the the serialized bytes for the `LocalStorageAreaWriteMetaData`
  // protobuf with `last_modified` and `total_size`.
  static Value CreateWriteMetaDataValue(base::Time last_modified,
                                        base::ByteSize total_size);

  // TODO(crbug.com/377242771): Make private after fully adopting the
  // `DomStorageDatabase` interface, which will make LevelDB and SQLite
  // swappable.
  //
  // Returns "_<storage key>\x00", which matches all of the map key/value pairs
  // for `storage_key`.
  static Key GetMapPrefix(const blink::StorageKey& storage_key);

  // Implement the `DomStorageDatabase` interface:
  DomStorageDatabaseLevelDB& GetLevelDB() override;

  // Reads the "META:" and "METACCESS" entries from the LevelDB database
  // described above. Parses the following from each entry to create a
  // `MapMetadata` that combines:
  //
  //  (1) A storage key from the entry's key.
  //
  //  (2) The last write time and total bytes from the "META:" entry's value,
  //      which is a `LocalStorageAreaWriteMetaData` protobuf.
  //
  //  (3) The last access time from the "METAACCESS:" entry's value, which is a
  //      `LocalStorageAreaAccessMetaData` protobuf.
  StatusOr<Metadata> ReadAllMetadata() override;

  // Writes LevelDB entries for map usage metadata.  Writes up to two entries
  // for each map in `metadata.map_metadata`:
  //
  // (1) Writes a "META:" entry when the map's usage contains a last modified
  //     time and total size.
  //
  // (2) Writes a "METAACCESS:" entry when the map's usage contains a last
  //     accessed time.
  DbStatus PutMetadata(Metadata metadata) override;

  // Removes the following for each storage key:
  //
  // (1) All of the map's key/value pairs using the prefix "_<storage key>\x00".
  //
  // (2) The "META:<storage key>" entry, containing the map's last modified time
  //     and total size.
  //
  // (3) The "METAACCESS:<storage key>" entry, containing the map's last
  //     accessed time.
  DbStatus DeleteStorageKeysFromSession(
      std::string session_id,
      std::vector<blink::StorageKey> storage_keys,
      absl::flat_hash_set<int64_t> excluded_cloned_map_ids) override;
  DbStatus RewriteDB() override;

  // Test-only functions.
  void MakeAllCommitsFailForTesting() override;
  void SetDestructionCallbackForTesting(base::OnceClosure callback) override;

 private:
  std::unique_ptr<DomStorageDatabaseLevelDB> leveldb_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_LOCAL_STORAGE_LEVELDB_H_
