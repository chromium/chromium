// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_H_

#include <stdint.h>

#include <map>
#include <memory>
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
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/types/pass_key.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_histogram_helper.h"
#include "components/services/storage/dom_storage/dom_storage_rollout.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace base {
class FilePath;
namespace trace_event {
class ProcessMemoryDump;
}  // namespace trace_event
}  // namespace base

namespace sql {
class Database;
}  // namespace sql

namespace storage {
enum class StorageType;

// Abstract interface for DOM storage database implementations. Provides
// key-value storage operations for DOMStorage StorageAreas.
//
// Two instances of this database exists per Profile: one for session storage
// and one for local storage. Records the key-value pairs for all StorageAreas
// along with usage metadata.
//
// Use the `DomStorageDatabaseFactory` to asynchronously create and open an
// instance of this type from any sequence. When owning the resulting
// `SequenceBound<std::unique_ptr<DomStorageDatabase>>`, all work on the
// `DomStorageDatabase` can be safely done via `AsyncCall()` /
// `PostTaskWithThisObject()`.
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
  // Local storage does not use `session_id`.  Instead, local storage contains a
  // single global session where each `storage_key` owns one map of key value
  // pairs.
  //
  // In session storage, each map must have at least one `session_id`. The
  // number of sessions consuming a map can increase or decrease. A session can
  // clone a map, which then shares the same map across multiple sessions.
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
    // Construct a map locator for the global session in local storage.
    explicit MapLocator(blink::StorageKey storage_key);
    MapLocator(blink::StorageKey storage_key, int64_t map_id);

    // Construct a map locator for a specific `session_id` in session storage.
    MapLocator(std::string session_id, blink::StorageKey storage_key);
    MapLocator(std::string session_id,
               blink::StorageKey storage_key,
               int64_t map_id);

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

    // For debug logging.  Returns all members in the following string format:
    // "sessions_ids:<session_ids_[0]>:<session_ids_[1]>:...<session_ids_[N]>,
    // storage_key:<storage_key_>, map_id: <map_id_>".
    std::string ToDebugString() const;

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
  };

  // A collection of key/value pair updates for a single map. Optionally
  // contains map metadata to update like last modified time.
  struct MapBatchUpdate {
    explicit MapBatchUpdate(MapLocator map_to_update);
    ~MapBatchUpdate();

    MapBatchUpdate(MapBatchUpdate&&);
    MapBatchUpdate& operator=(MapBatchUpdate&&);

    // Support move-only.
    MapBatchUpdate(const MapBatchUpdate&) = delete;
    MapBatchUpdate& operator=(const MapBatchUpdate&) = delete;

    // The map to update.
    MapLocator map_locator;

    // Applications use the following JavaScript APIs to manipulate persisted
    // map key/value pairs.
    //
    // `Storage::clear()` deletes all key/value pairs.
    bool clear_all_first = false;

    // `Storage::setItem()` adds or updates a key/value pair.
    std::vector<KeyValuePair> entries_to_add;

    // `Storage::removeItem()` deletes a key/value pair.
    std::vector<Key> keys_to_delete;

    // The map's optional usage metadata to persist along with this update. Use
    // `should_delete_all_usage_` to remove the map's usage metadata instead of
    // persisting new metadata.
    //
    // Session storage does not record usage metadata, leaving `map_usage` below
    // null.
    //
    // Local storage records last accessed time once per map load either during
    // the first update of a key/value pair or during the unloading of the map.
    // Every local storage key/value pair update must record a new last modified
    // time and a new total map size.  When a map becomes empty with no
    // key/value pairs remaining, the empty map deletes its usage metadata from
    // the database.
    class Usage {
     public:
      std::optional<base::Time> last_accessed() const { return last_accessed_; }
      std::optional<base::Time> last_modified() const { return last_modified_; }
      std::optional<base::ByteSize> total_size() const { return total_size_; }

      bool should_delete_all_usage() const { return should_delete_all_usage_; }

      void SetLastAccessed(base::Time last_accessed) {
        CHECK(!should_delete_all_usage_);
        last_accessed_ = last_accessed;
      }

      void SetLastModifiedAndTotalSize(base::Time last_modified,
                                       base::ByteSize total_size) {
        CHECK(!should_delete_all_usage_);
        last_modified_ = last_modified;
        total_size_ = total_size;
      }

      void DeleteAllUsage() {
        CHECK(!last_accessed_);
        CHECK(!last_modified_);
        should_delete_all_usage_ = true;
      }

     private:
      std::optional<base::Time> last_accessed_;
      std::optional<base::Time> last_modified_;
      std::optional<base::ByteSize> total_size_;

      // Set to true to delete the map's last accessed time, last modified time
      // and total size from the database. When true, all other members must be
      // `std::nullopt`.
      bool should_delete_all_usage_ = false;
    };
    std::optional<Usage> map_usage;
  };

  // Returns the LevelDB database path for `storage_type` under
  // `storage_partition_dir`. The path is a directory:
  //
  // `storage_partition_dir`/Local Storage/leveldb     (for `kLocalStorage`)
  // `storage_partition_dir`/Session Storage           (for `kSessionStorage`)
  static base::FilePath GetLevelDbPath(
      StorageType storage_type,
      const base::FilePath& storage_partition_dir);

  // Returns the SQLite database path for `storage_type` under
  // `storage_partition_dir`. The path is a file:
  //
  // `storage_partition_dir`/LocalStorage              (for `kLocalStorage`)
  // `storage_partition_dir`/SessionStorage            (for `kSessionStorage`)
  static base::FilePath GetSqlitePath(
      StorageType storage_type,
      const base::FilePath& storage_partition_dir);

  virtual ~DomStorageDatabase() = default;

  // Opens an on-disk or in-memory database and returns the result. To create an
  // in-memory database, provide an empty `database_path`.
  virtual DbStatus Open(
      const base::FilePath& database_path,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id) = 0;

  // Gets an entire map's key/value pairs.
  virtual StatusOr<std::map<Key, Value>> ReadMapKeyValues(
      MapLocator map_locator) = 0;

  // Persist all `map_updates`.  Each update adds, modifies and/or deletes
  // key/value pairs in a map.  Updates optionally includes map usage metadata
  // to persist like last modified time.
  virtual DbStatus UpdateMaps(std::vector<MapBatchUpdate> map_updates) = 0;

  // Deep copies a map's key/value pairs from one session to another.
  virtual DbStatus CloneMap(MapLocator source_map, MapLocator target_map) = 0;

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

  // Removes all traces of deleted data from the backing storage.  For example,
  // removes all traces of an origin URL that might exist in the deleted data.
  //
  // This can be expensive. For SQLite, it runs a checkpoint that transfers
  // content from the WAL file to the database file.  For LevelDB, it creates a
  // new copy of the database that replaces the old copy.
  virtual DbStatus CleanUpStaleData() = 0;

  // Test-only functions.
  virtual DbStatus PutVersionForTesting(int64_t version) = 0;
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

  struct DestroyOutcome {
    DbStatus status;
    DatabaseMetricsType destroyed_db_metrics_type;
  };

  // Result of opening a DomStorage database. Contains everything
  // the caller needs to use the database and record metrics. The database has
  // already been opened on its backend sequence. `open_status` carries the
  // outcome.
  class OpenResult {
   public:
    OpenResult();
    ~OpenResult();
    OpenResult(OpenResult&&);
    OpenResult& operator=(OpenResult&&);

    // Stores `database` for transport to the caller's sequence, with a deleter
    // that destroys it on `task_runner` (its backend sequence) if the result is
    // dropped before `TakeDatabase()`. Producers of an `OpenResult` must set
    // the database via this.
    void SetDatabase(scoped_refptr<base::SequencedTaskRunner> task_runner,
                     std::unique_ptr<DomStorageDatabase> database);

    // Returns the database stored by `SetDatabase()`, bound to its backend
    // sequence. Must be called once, on the caller's sequence.
    base::SequenceBound<std::unique_ptr<DomStorageDatabase>> TakeDatabase();

    base::FilePath database_path;
    DatabaseMetricsType metrics_type = DatabaseMetricsType::kOnDisk;
    bool is_sqlite = false;
    DbStatus open_status = DbStatus::IOError("uninitialized");

    // Set only when the open was requested with a non-empty `dir_to_destroy`,
    // reporting the outcome of destroying the pre-existing on-disk database
    // before opening. Consumed by recovery callers to record recovery
    // telemetry.
    std::optional<DestroyOutcome> destroy_outcome;

   private:
    // The opened database while the result is in flight to the caller's
    // sequence. If the result is dropped before `TakeDatabase()`, the
    // `OnTaskRunnerDeleter` destroys it on its backend sequence, closing its
    // file handle.
    std::unique_ptr<DomStorageDatabase, base::OnTaskRunnerDeleter> database_{
        nullptr, base::OnTaskRunnerDeleter(nullptr)};
  };

  using OpenResultCallback = base::OnceCallback<void(OpenResult)>;

  using OpenCallback = base::RepeatingCallback<void(
      StorageType,
      const base::FilePath& dir_to_open,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&,
      const base::FilePath& dir_to_destroy,
      OpenResultCallback)>;

  // Asynchronously opens a DomStorageDatabase. The callback receives the opened
  // database, its resolved configuration, and the Open() outcome.
  //
  // The new database is opened at `dir_to_open`, or in-memory if `dir_to_open`
  // is empty.
  //
  // If `dir_to_destroy` is non-empty, the pre-existing on-disk database at that
  // location is destroyed before the new database is opened, all within this
  // flow. The destroy outcome is reported via `OpenResult::destroy_status`.
  // `dir_to_destroy` may differ from `dir_to_open`. E.g. recovery can destroy
  // the on-disk database, then open an in-memory database by passing the
  // on-disk location as `dir_to_destroy` and an empty `dir_to_open`.
  static void Open(
      StorageType storage_type,
      const base::FilePath& dir_to_open,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      const base::FilePath& dir_to_destroy,
      OpenResultCallback callback);

 private:
  friend class LocalStorageLevelDBTest;
  friend class LocalStorageSqliteTest;
  friend class LocalStorageImplOnDiskSQLiteRolloutTestBase;
  friend class DomStorageDatabaseTest;
  friend class SessionStorageLevelDBTest;
  friend class SessionStorageSqliteTest;
  friend class SessionStorageImplOnDiskSQLiteRolloutTestBase;
  friend class ScopedDomStorageDatabaseFactoryForTesting;

  static void OpenImpl(
      StorageType storage_type,
      const base::FilePath& dir_to_open,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      const base::FilePath& dir_to_destroy,
      OpenResultCallback callback);

  // The following Open helpers are private static members so that
  // `InitializeDatabase` can create a `PassKey` when constructing backend
  // subclasses.

  using OnDiskStateCheckedCallback =
      base::OnceCallback<void(LevelDbOnDiskState)>;

  // Reads the on-disk LevelDB state at `dir` and passes it to `callback`. Runs
  // the read on `dir`'s LevelDB blocking sequence, re-posting to it if needed.
  static void CheckOnDiskLevelDbState(StorageType storage_type,
                                      base::FilePath dir,
                                      OnDiskStateCheckedCallback callback);

  using OnDatabaseDestroyedCallback =
      base::OnceCallback<void(std::optional<DestroyOutcome> destroy_outcome)>;

  // Bound as the `CheckOnDiskLevelDbState()` continuation for the destroy flow,
  // so `leveldb_state` trails. Like `InitializeDatabase`, this runs on the
  // destroy target's blocking sequence (selected via `GetTaskRunnerForDb()`),
  // re-posting to it if needed.
  static void DestroyDatabase(StorageType storage_type,
                              base::FilePath dir_to_destroy,
                              OnDatabaseDestroyedCallback callback,
                              LevelDbOnDiskState leveldb_state);

  // Invoked with the resolved backend to construct and open the database.
  using OnBackendResolvedCallback =
      base::OnceCallback<void(bool is_sqlite,
                              bool write_exp_tag,
                              DatabaseMetricsType metrics_type,
                              base::FilePath database_path,
                              std::optional<DestroyOutcome> destroy_outcome)>;

  // Resolves the backend (in-memory vs on-disk, SQLite vs LevelDB) and the
  // database path using `dir_to_open` (empty for in-memory), then runs
  // `callback` with the result.
  static void ResolveDatabaseBackend(
      StorageType storage_type,
      base::FilePath dir_to_open,
      OnBackendResolvedCallback callback,
      std::optional<DestroyOutcome> destroy_outcome);

  // Resolves the experimental backend from the on-disk LevelDB state, then runs
  // `callback`. Bound as the `CheckOnDiskLevelDbState()` continuation for
  // the open flow, so `leveldb_state` (the produced on-disk state) trails.
  static void ResolveOnDiskExperimentalDatabaseBackend(
      StorageType storage_type,
      base::FilePath dir_to_open,
      DomStorageSqliteRolloutStage stage,
      OnBackendResolvedCallback callback,
      std::optional<DestroyOutcome> destroy_outcome,
      LevelDbOnDiskState leveldb_state);

  // Constructs the resolved backend, opens it, records the OpenDatabase
  // histograms, and runs `callback` (already bound to the caller's sequence)
  // with the assembled `OpenResult`.
  //
  // For on-disk databases this may be called from any sequence. The work runs
  // on the backend's blocking sequence (selected via `GetTaskRunnerForDb()`),
  // re-posting to it if needed. For in-memory databases the caller must invoke
  // this on the backend's sequence, since `GetTaskRunnerForDb()` returns a
  // fresh sequence on each call for an empty path and so cannot be used here to
  // re-post.
  static void InitializeDatabase(
      StorageType storage_type,
      std::optional<base::trace_event::MemoryAllocatorDumpGuid> memory_dump_id,
      OpenResultCallback callback,
      bool is_sqlite,
      bool write_exp_tag,
      DatabaseMetricsType metrics_type,
      base::FilePath database_path,
      std::optional<DestroyOutcome> destroy_outcome);

  // Returns the open callback, lazily initialized on first call. Defaults to
  // `OpenImpl`. Tests can swap in a custom implementation via
  // `ScopedDomStorageDatabaseFactoryForTesting`.
  static OpenCallback& GetOpenCallback();

  // Allow unit tests to create a database instance without `SequenceBound`.
  static PassKey CreatePassKeyForTesting();
};

// Creates a blocking task runner to use for `database_path`.  `database_path`
// will be empty for in memory databases.
scoped_refptr<base::SequencedTaskRunner> GetTaskRunnerForDb(
    const base::FilePath& database_path);

// Reports the memory usage of `database` into `pmd`. Creates an allocator
// dump named `dump_name` and links it to the shared global dump identified by
// `memory_dump_id`.
void ReportDatabaseMemoryUsage(
    sql::Database* database,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    base::trace_event::ProcessMemoryDump* pmd,
    std::string dump_name);

// A shared implementation of `DomStorageDatabase::PurgeOrigins()` from above.
// Both LevelDB and SQLite implementations use this helper function.
DbStatus PurgeOrigins(DomStorageDatabase& database,
                      std::set<url::Origin> origins);

// Migrates all metadata and map entries from `source` to `destination`.
// Intended for migrating from LevelDB to SQLite. The `destination` must be
// empty.
DbStatus MigrateDatabase(DomStorageDatabase& source,
                         DomStorageDatabase& destination);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_H_
