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
  // base::SequenceBound<DomStorageDatabase>.
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
  DbStatus RewriteDB() override;

  // Test-only functions.
  bool ShouldFailAllCommits() override;
  void MakeAllCommitsFailForTesting() override;
  void SetDestructionCallbackForTesting(base::OnceClosure callback) override;

 private:
  std::unique_ptr<DomStorageDatabaseLevelDB> leveldb_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_SESSION_STORAGE_LEVELDB_H_
