// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_SESSION_STORAGE_LEVELDB_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_SESSION_STORAGE_LEVELDB_H_

#include <memory>
#include <optional>
#include <string>

#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/types/pass_key.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"

namespace storage {

// The "namespace-" prefix for all metadata entries.
inline constexpr const uint8_t kNamespacePrefix[] = {'n', 'a', 'm', 'e', 's',
                                                     'p', 'a', 'c', 'e', '-'};

// For metadata keys, splits the session id and storage key:
// "namespace-<session_id>-<storage_key>".
inline constexpr const uint8_t kNamespaceStorageKeySeparator = '-';

// The "next-map-id" key.
inline constexpr const uint8_t kNextMapIdKey[] = {'n', 'e', 'x', 't', '-', 'm',
                                                  'a', 'p', '-', 'i', 'd'};

// The schema "version" key.
inline constexpr const uint8_t kSessionStorageLevelDBVersionKey[] = {
    'v', 'e', 'r', 's', 'i', 'o', 'n'};

// LevelDB supports one schema version for session storage without migration.
inline constexpr int64_t kSessionStorageLevelDBVersion = 1;

// Reads and writes entries in the session storage LevelDB database with the
// following schema:
//
// | key                             | value | notes
// |---------------------------------|-------|----------------------------------
// | map-1-a                         | b     | key "a" = value "b" in map 1.
// | map-2-c                         | d     |
// | map-2-foo                       | bar   |
// | ...                             | ...   |
// | namespace-<guid1>-<StorageKey1> | 1     | <guid1>-<StorageKey1> uses map 1.
// | namespace-<guid1>-<StorageKey2> | 2     |
// | namespace-<guid2>-<StorageKey1> | 3     |
// | namespace-<guid3>-<StorageKey1> | 1     | <guid3>-<StorageKey1> is a
// | ...                             | ...   | shallow clone of map 1.
// | next-map-id                     | 4     |
// | version                         | 1     |
//
// Example session key:
//   namespace-dabc53e1_8291_4de5_824f_dab8aa69c846-https://example.com/
//
// Data types:
//  - GUIDs: 36 character strings for the session IDs.
//  - Numbers: Converted to text strings (the integer 2 becomes the string "2").
//  - Map keys from JavaScript: Converted to UTF-8.
//  - Map values from JavaScript: Remain UTF-16.
class SessionStorageLevelDB : public DomStorageDatabase {
 private:
  using PassKey = base::PassKey<DomStorageDatabaseFactory>;

 public:
  // Use `DomStorageDatabaseFactory::Open()` to construct a
  // `base::SequenceBound<DomStorageDatabase>`.
  explicit SessionStorageLevelDB(PassKey);
  ~SessionStorageLevelDB() override;

  // Opens an on-disk or in-memory LevelDB and returns the result. To create an
  // in-memory database, provide an empty `directory`.
  DbStatus Open(PassKey,
                const base::FilePath& directory,
                const std::string& name,
                const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
                    memory_dump_id);

  SessionStorageLevelDB(const SessionStorageLevelDB&) = delete;
  SessionStorageLevelDB& operator=(const SessionStorageLevelDB&) = delete;

  // Implement the `DomStorageDatabase` interface:
  DomStorageDatabaseLevelDB& GetLevelDB() override;
  StatusOr<Metadata> ReadAllMetadata() override;
  DbStatus PutMetadata(Metadata metadata) override;
  DbStatus DeleteStorageKeysFromSession(
      std::string session_id,
      std::vector<blink::StorageKey> storage_keys,
      absl::flat_hash_set<int64_t> excluded_cloned_map_ids) override;
  DbStatus RewriteDB() override;

  // Test-only functions.
  void MakeAllCommitsFailForTesting() override;
  void SetDestructionCallbackForTesting(base::OnceClosure callback) override;

  // Returns `namespace-<session_id>-<storage_key>`.
  //
  // TODO(crbug.com/377242771): Make private after refactoring to support a
  // swappable backend for SQLite.
  static Key CreateMapMetadataKey(std::string session_id,
                                  const blink::StorageKey& storage_key);

 private:
  // Parses the value from the next map ID key in the LevelDB.  Converts the
  // value from a integer text string like "234" to an `int64_t`.  Returns 0
  // as the default value to use when the next map key does not exist in the
  // LevelDB.  The next map ID determines the next available ID for a new map to
  // use.
  StatusOr<int64_t> ReadNextMapId() const;

  // Parses all "namespace-" entries.  Each key contains a session ID and
  // storage key.  Each value contains the map ID integer text string.  Combines
  // these to create a `MapLocator` in a `MapMetadata` for each "namespace-"
  // entry.
  StatusOr<std::vector<DomStorageDatabase::MapMetadata>> ReadAllMapMetadata()
      const;

  std::unique_ptr<DomStorageDatabaseLevelDB> leveldb_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_SESSION_STORAGE_LEVELDB_H_
