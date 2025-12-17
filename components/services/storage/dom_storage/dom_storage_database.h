// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "storage/common/database/db_status.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace base {
class FilePath;
namespace trace_event {
class MemoryAllocatorDumpGuid;
}  // namespace trace_event
}  // namespace base

namespace storage {
class DomStorageDatabaseLevelDB;

// Abstract interface for DOM storage database implementations. Provides
// key-value storage operations for DOMStorage StorageAreas.
//
// Two instances of this database exists per Profile: one for session storage
// and one for local storage. Records the key-value pairs for all StorageAreas
// along with usage metadata.
//
// Use the `DomStorageDatabaseFactory` to asynchronously create an instance of
// this type from any sequence. When owning a SequenceBound<DomStorageDatabase>
// as produced by those helpers, all work on the DomStorageDatabase can be
// safely done via `SequenceBound::PostTaskWithThisObject`.
class DomStorageDatabase {
 public:
  using Key = std::vector<uint8_t>;
  using KeyView = base::span<const uint8_t>;
  using Value = std::vector<uint8_t>;
  using ValueView = base::span<const uint8_t>;

  struct KeyValuePair {
    KeyValuePair();
    KeyValuePair(KeyValuePair&&);
    KeyValuePair(const KeyValuePair&);
    KeyValuePair(Key key, Value value);
    ~KeyValuePair();
    KeyValuePair& operator=(KeyValuePair&&);
    KeyValuePair& operator=(const KeyValuePair&);

    bool operator==(const KeyValuePair& rhs) const;

    Key key;
    Value value;
  };

  // Locates a map of persisted key value pairs in the database. Use `map_id`
  // to find the map data. Use `session_id` and `storage_key` to find the
  // `map_id`. Some maps are loaded on demand where `map_id` remains unknown
  // until the first read or write.
  //
  // The number of sessions consuming a map can increase or decrease. A session
  // can clone a map, which then shares the same map across multiple sessions.
  // Cloned maps have at least 2 IDs in `session_ids_`. A session may also stop
  // using a map by deleting it or forking it, which then removes an ID from
  // `session_ids_`. `session_ids_` is empty for an unused map.
  //
  // Maps are read-only when used by multiple sessions. To modify a cloned
  // map, a session must first create a new forked copy, which avoids
  // modifying the clone's key/value pairs in other sessions.
  //
  // Maps without sessions are not in use. They can be deleted.
  class MapLocator {
   public:
    MapLocator(std::string source_session_id,
               blink::StorageKey source_storage_key);
    MapLocator(std::string source_session_id,
               blink::StorageKey source_storage_key,
               int64_t source_map_id);
    ~MapLocator();

    MapLocator(MapLocator&&);
    MapLocator& operator=(MapLocator&&);

    // Support move-only.
    MapLocator(const MapLocator&) = delete;
    MapLocator& operator=(const MapLocator&) = delete;

    const std::vector<std::string>& session_ids() const;
    const blink::StorageKey& storage_key() const;
    std::optional<int64_t> map_id() const;

    void AddSession(std::string session_id);
    void RemoveSession(const std::string& session_id);

    MapLocator Clone() const;

   private:
    MapLocator();

    std::vector<std::string> session_ids_;
    blink::StorageKey storage_key_;
    std::optional<int64_t> map_id_;
  };

  // Cloned sessions share the same underlying map.
  //
  // TODO(crbug.com/469468099): Refactor to remove `SharedMapLocator` and
  // reference counting.
  class SharedMapLocator : public MapLocator,
                           public base::RefCounted<SharedMapLocator> {
   public:
    explicit SharedMapLocator(MapLocator source);

   private:
    friend class base::RefCounted<SharedMapLocator>;
    ~SharedMapLocator();
  };

  // Describes a consumer of a persisted map's data and its size and usage. Some
  // `DomStorageDatabase` implementors don't record usage. For brand new empty
  // maps, metadata for `last_accessed` might exist while `last_modified` and
  // `total_size` might NOT exist until after the first write.
  struct MapMetadata {
    MapLocator map_locator;

    std::optional<base::Time> last_accessed;
    std::optional<base::Time> last_modified;
    std::optional<base::ByteSize> total_size;
  };

  // Describes all metadata in the database.
  struct Metadata {
    Metadata();
    explicit Metadata(std::vector<MapMetadata> source_map_metadata);
    ~Metadata();

    Metadata(Metadata&&);
    Metadata& operator=(Metadata&&);

    // Support move-only.
    Metadata(const Metadata&) = delete;
    Metadata& operator=(const Metadata&) = delete;

    std::vector<MapMetadata> map_metadata;
    std::optional<int64_t> next_map_id;
  };

  virtual ~DomStorageDatabase() = default;

  // TODO(crbug.com/377242771): Remove LevelDB accessor after fully migrating to
  // this interface.
  virtual DomStorageDatabaseLevelDB& GetLevelDB() = 0;

  // TODO(crbug.com/377242771): Support both SQLite and LevelDB by adding more
  // shared functions to this interface.
  //
  // Gets an entire map's key/value pairs.
  virtual StatusOr<std::map<Key, Value>> ReadMapKeyValues(
      MapLocator map_locator) = 0;

  // Get all map locators along with their size and usage. Also gets the next
  // available map id that the database will assign to a newly created map.
  virtual StatusOr<Metadata> ReadAllMetadata() = 0;

  // Put `metadata` in the database. Overwrites existing values if present.  For
  // example, if `metadata.map_metadata` contains map X then `PutMetadata()`
  // will replace map X's metadata in the database.
  virtual DbStatus PutMetadata(Metadata metadata) = 0;

  // In `session_id`, deletes the metadata and optionally the map for each
  // provided storage key.  Use `maps_to_delete` to specify which map key/value
  // pairs to remove.  Callers must not delete maps still in use by other
  // cloned sessions.
  virtual DbStatus DeleteStorageKeysFromSession(
      std::string session_id,
      std::vector<blink::StorageKey> metadata_to_delete,
      std::vector<MapLocator> maps_to_delete) = 0;

  // Deletes the metadata for each storage key that belongs to a session in
  // `session_ids`. Optionally deletes map key/value pairs using
  // `maps_to_delete` to specify what to remove. Callers must not delete maps
  // still referenced by other cloned sessions.
  virtual DbStatus DeleteSessions(std::vector<std::string> session_ids,
                                  std::vector<MapLocator> maps_to_delete) = 0;

  // Deletes all data if its origin is in `origins`, or if it is third-party and
  // the top-level site is same-site with one of those origins.
  virtual DbStatus PurgeOrigins(std::set<url::Origin> origins) = 0;

  // For LevelDB only. Rewrites the database on disk to
  // clean up traces of deleted entries.
  //
  // NOTE: If `RewriteDB()` fails, this DomStorageDatabase may no longer
  // be usable; in such cases, all future operations will return an IOError
  // status.
  virtual DbStatus RewriteDB() = 0;

  // Test-only functions.
  virtual void MakeAllCommitsFailForTesting() = 0;
  virtual void SetDestructionCallbackForTesting(base::OnceClosure callback) = 0;
};

// Required for the LevelDB implementation, which has separate schemas for
// local storage and session storage.
enum class StorageType {
  kLocalStorage,
  kSessionStorage,
};

class DomStorageDatabaseFactory {
 public:
  using PassKey = base::PassKey<DomStorageDatabaseFactory>;

  using OpenCallback = base::OnceCallback<void(
      StatusOr<base::SequenceBound<DomStorageDatabase>> database)>;

  // Creates and opens a `SequenceBound<DomStorageDatabase>` using
  // `blocking_task_runner`. Runs `callback` with result after opening the
  // database.
  //
  // To create an in-memory database, provide an empty `directory`.
  static void Open(
      StorageType storage_type,
      const base::FilePath& directory,
      const std::string& name,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      OpenCallback callback);

  // Destroys the persistent database named `name` within the filesystem
  // directory identified by the absolute path in `directory`.
  //
  // All work is done on `task_runner`, which must support blocking operations,
  // and upon completion `callback` is called on the calling sequence.
  static void Destroy(
      const base::FilePath& directory,
      const std::string& name,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      base::OnceCallback<void(DbStatus)> callback);

 private:
  friend class LocalStorageLevelDBTest;
  friend class SessionStorageLevelDBTest;

  // `Open()` uses this function to asynchronously create a
  // `base::SequenceBound<DomStorageDatabase>`. The `TDatabase` template
  // specifies the derived type to construct like `LocalStorageLevelDB`. The
  // derived type must inherit the `DomStorageDatabase` interface. After
  // failure, `callback` runs with an error `status`.
  template <typename TDatabase>
  static void CreateSequenceBoundDomStorageDatabase(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const base::FilePath& directory,
      const std::string& name,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      base::OnceCallback<
          void(StatusOr<base::SequenceBound<TDatabase>> database)> callback);

  // Allow unit tests to create a database instance without `SequenceBound`.
  static PassKey CreatePassKeyForTesting();
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_H_
