// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_database.h"

#include "base/debug/leak_annotations.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/types/expected_macros.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/local_storage_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/session_storage_leveldb.h"
#include "components/services/storage/dom_storage/sqlite/local_storage_sqlite.h"
#include "components/services/storage/dom_storage/sqlite/session_storage_sqlite.h"
#include "components/services/storage/dom_storage/sqlite/sqlite_database_utils.h"
#include "components/services/storage/public/cpp/constants.h"
#include "sql/database.h"

namespace storage {
namespace {

// Constructs an absolute path to the session storage database using
// `storage_partition_dir`.  For LevelDB, the path is a directory:
//
// `storage_partition_dir`/Session Storage
//
// When the `kDomStorageSqlite` feature flag is enabled, the path is a file:
//
// `storage_partition_dir`/SessionStorage
base::FilePath GetSessionStorageDatabasePath(
    const base::FilePath& storage_partition_dir) {
  CHECK(!storage_partition_dir.empty());
  CHECK(storage_partition_dir.IsAbsolute());

  if (base::FeatureList::IsEnabled(kDomStorageSqlite)) {
    return storage_partition_dir.AppendASCII("SessionStorage");
  }
  return storage_partition_dir.AppendASCII("Session Storage");
}

}  // namespace

DomStorageDatabase::KeyValuePair::KeyValuePair() = default;

DomStorageDatabase::KeyValuePair::~KeyValuePair() = default;

DomStorageDatabase::KeyValuePair::KeyValuePair(KeyValuePair&&) = default;

DomStorageDatabase::KeyValuePair::KeyValuePair(const KeyValuePair&) = default;

DomStorageDatabase::KeyValuePair::KeyValuePair(Key key, Value value)
    : key(std::move(key)), value(std::move(value)) {}

DomStorageDatabase::KeyValuePair& DomStorageDatabase::KeyValuePair::operator=(
    KeyValuePair&&) = default;

DomStorageDatabase::KeyValuePair& DomStorageDatabase::KeyValuePair::operator=(
    const KeyValuePair&) = default;

bool DomStorageDatabase::KeyValuePair::operator==(
    const KeyValuePair& rhs) const {
  return std::tie(key, value) == std::tie(rhs.key, rhs.value);
}

DomStorageDatabase::MapLocator::MapLocator(blink::StorageKey storage_key)
    : storage_key_(storage_key) {}

DomStorageDatabase::MapLocator::MapLocator(blink::StorageKey storage_key,
                                           int64_t map_id)
    : storage_key_(storage_key), map_id_(map_id) {}

DomStorageDatabase::MapLocator::MapLocator(std::string session_id,
                                           blink::StorageKey storage_key)
    : storage_key_(storage_key) {
  session_ids_.push_back(std::move(session_id));
}

DomStorageDatabase::MapLocator::MapLocator(std::string session_id,
                                           blink::StorageKey storage_key,
                                           int64_t map_id)
    : storage_key_(storage_key), map_id_(map_id) {
  session_ids_.push_back(std::move(session_id));
}

DomStorageDatabase::MapLocator::~MapLocator() = default;

DomStorageDatabase::MapLocator::MapLocator(MapLocator&&) = default;

DomStorageDatabase::MapLocator& DomStorageDatabase::MapLocator::operator=(
    MapLocator&&) = default;

const blink::StorageKey& DomStorageDatabase::MapLocator::storage_key() const {
  return storage_key_;
}

const std::vector<std::string>& DomStorageDatabase::MapLocator::session_ids()
    const {
  return session_ids_;
}

std::optional<int64_t> DomStorageDatabase::MapLocator::map_id() const {
  return map_id_;
}

void DomStorageDatabase::MapLocator::AddSession(std::string session_id) {
  session_ids_.push_back(std::move(session_id));
}

void DomStorageDatabase::MapLocator::RemoveSession(
    const std::string& session_id) {
  std::erase(session_ids_, session_id);
}

DomStorageDatabase::MapLocator DomStorageDatabase::MapLocator::Clone() const {
  MapLocator clone;
  clone.session_ids_ = session_ids_;
  clone.storage_key_ = storage_key_;
  clone.map_id_ = map_id_;
  return clone;
}

std::string DomStorageDatabase::MapLocator::ToDebugString() const {
  std::string sessions = base::JoinString(session_ids_, /*separator=*/":");
  std::string map_id = map_id_ ? base::NumberToString(*map_id_) : "null";

  return base::StringPrintf("sessions_ids:%s, storage_key:%s, map_id:%s",
                            sessions, storage_key_.GetDebugString(), map_id);
}

DomStorageDatabase::MapLocator::MapLocator() = default;

DomStorageDatabase::SharedMapLocator::SharedMapLocator(MapLocator source)
    : MapLocator(std::move(source)) {}

DomStorageDatabase::SharedMapLocator::~SharedMapLocator() = default;

DomStorageDatabase::Metadata::Metadata() = default;

DomStorageDatabase::Metadata::Metadata(
    std::vector<MapMetadata> source_map_metadata)
    : map_metadata(std::move(source_map_metadata)) {}

DomStorageDatabase::Metadata::~Metadata() = default;

DomStorageDatabase::Metadata::Metadata(Metadata&&) = default;

DomStorageDatabase::Metadata& DomStorageDatabase::Metadata::operator=(
    Metadata&&) = default;

DomStorageDatabase::MapBatchUpdate::MapBatchUpdate(MapLocator map_to_update)
    : map_locator{std::move(map_to_update)} {}

DomStorageDatabase::MapBatchUpdate::~MapBatchUpdate() = default;

DomStorageDatabase::MapBatchUpdate::MapBatchUpdate(MapBatchUpdate&&) = default;

DomStorageDatabase::MapBatchUpdate&
DomStorageDatabase::MapBatchUpdate::operator=(MapBatchUpdate&&) = default;

base::FilePath DomStorageDatabase::GetPath(
    StorageType storage_type,
    const base::FilePath& storage_partition_dir) {
  switch (storage_type) {
    case StorageType::kLocalStorage:
      return GetLocalStorageDatabasePath(storage_partition_dir);
    case StorageType::kSessionStorage:
      return GetSessionStorageDatabasePath(storage_partition_dir);
  }
  NOTREACHED();
}

// static
DomStorageDatabaseFactory::CreateCallback&
DomStorageDatabaseFactory::GetCreateCallback() {
  static base::NoDestructor<CreateCallback> callback(
      base::BindRepeating(&DomStorageDatabaseFactory::CreateImpl));
  return *callback;
}

// static
DomStorageDatabaseFactory::DestroyCallback&
DomStorageDatabaseFactory::GetDestroyCallback() {
  static base::NoDestructor<DestroyCallback> callback(
      base::BindRepeating(&DomStorageDatabaseFactory::DestroyImpl));
  return *callback;
}

// static
base::SequenceBound<DomStorageDatabase> DomStorageDatabaseFactory::Create(
    StorageType storage_type,
    bool is_in_memory,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  return GetCreateCallback().Run(storage_type, is_in_memory,
                                 std::move(blocking_task_runner));
}

// static
base::SequenceBound<DomStorageDatabase> DomStorageDatabaseFactory::CreateImpl(
    StorageType storage_type,
    bool is_in_memory,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  switch (storage_type) {
    case StorageType::kLocalStorage: {
      if (ShouldUseSqliteBackend(is_in_memory)) {
        return base::SequenceBound<LocalStorageSqlite>(
            std::move(blocking_task_runner), PassKey());
      }
      return base::SequenceBound<LocalStorageLevelDB>(
          std::move(blocking_task_runner), PassKey());
    }
    case StorageType::kSessionStorage: {
      if (ShouldUseSqliteBackend(is_in_memory)) {
        return base::SequenceBound<SessionStorageSqlite>(
            std::move(blocking_task_runner), PassKey());
      }
      return base::SequenceBound<SessionStorageLevelDB>(
          std::move(blocking_task_runner), PassKey());
    }
  }
  NOTREACHED();
}

// static
void DomStorageDatabaseFactory::Destroy(const base::FilePath& database_path,
                                        StatusCallback callback) {
  GetDestroyCallback().Run(database_path, std::move(callback));
}

// static
void DomStorageDatabaseFactory::DestroyImpl(const base::FilePath& database_path,
                                            StatusCallback callback) {
  CHECK(!database_path.empty());
  CHECK(database_path.IsAbsolute());

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      GetTaskRunnerForDb(database_path);

  base::OnceCallback<DbStatus()> destroy_database_callback;
  if (base::FeatureList::IsEnabled(kDomStorageSqlite)) {
    destroy_database_callback =
        base::BindOnce(&sqlite::DestroyDatabase, database_path);
  } else {
    destroy_database_callback =
        base::BindOnce(&DomStorageDatabaseLevelDB::Destroy, database_path);
  }
  blocking_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(destroy_database_callback), std::move(callback));
}

base::PassKey<DomStorageDatabaseFactory>
DomStorageDatabaseFactory::CreatePassKeyForTesting() {
  return base::PassKey<DomStorageDatabaseFactory>();
}

scoped_refptr<base::SequencedTaskRunner> GetTaskRunnerForDb(
    const base::FilePath& database_path) {
  if (database_path.empty()) {
    // For the in-memory case, blocking shutdown is only important to avoid
    // leaking the SequenceBound on shutdown (and triggering ASAN failures).
    return base::ThreadPool::CreateSequencedTaskRunner(
        {base::WithBaseSyncPrimitives(),
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  }

  //  This will always return the same task runner for a given `database_path`.
  return base::ThreadPool::CreateSequencedTaskRunnerForResource(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      database_path);
}

bool ReportDatabaseMemoryUsage(
    sql::Database* database,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    base::trace_event::ProcessMemoryDump* pmd,
    std::string dump_name) {
  if (!database || !memory_dump_id) {
    return true;
  }

  int memory_usage = database->GetMemoryUsage();
  if (memory_usage == 0) {
    return true;
  }

  auto* db_dump = pmd->CreateAllocatorDump(dump_name);
  db_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                     base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                     memory_usage);
  auto* global_dump = pmd->CreateSharedGlobalAllocatorDump(*memory_dump_id);
  pmd->AddOwnershipEdge(global_dump->guid(), db_dump->guid());
  global_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                         base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                         memory_usage);
  return true;
}

DbStatus PurgeOrigins(DomStorageDatabase& database,
                      std::set<url::Origin> origins) {
  ASSIGN_OR_RETURN(DomStorageDatabase::Metadata all_metadata,
                   database.ReadAllMetadata());

  std::vector<blink::StorageKey> metadata_to_delete;
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;

  for (const DomStorageDatabase::MapMetadata& metadata :
       all_metadata.map_metadata) {
    // Ideally we would be recording last_accessed instead, but there is no
    // historical data on that. Instead, we will use last_modified as a sanity
    // check against other data as we try to understand how many 'old' storage
    // buckets are still in use. This is split into two buckets for greater
    // resolution on near and far term ages.
    if (metadata.last_modified && *metadata.last_modified < base::Time::Now()) {
      const int days_since_last_modified =
          (base::Time::Now() - *metadata.last_modified).InDays();
      base::UmaHistogramCustomCounts("LocalStorage.DaysSinceLastModified",
                                     days_since_last_modified, 1,
                                     kLocalStorageStaleBucketCutoffInDays, 100);
    }

    const blink::StorageKey& storage_key = metadata.map_locator.storage_key();

    for (const url::Origin& origin : origins) {
      if (storage_key.origin() == origin ||
          (storage_key.IsThirdPartyContext() &&
           storage_key.top_level_site().IsSameSiteWith(origin))) {
        metadata_to_delete.push_back(storage_key);
        maps_to_delete.emplace_back(storage_key);
        break;
      }
    }
  }
  return database.DeleteStorageKeysFromSession(/*session_id=*/std::string(),
                                               std::move(metadata_to_delete),
                                               std::move(maps_to_delete));
}

DbStatus MigrateDatabase(DomStorageDatabase& source,
                         DomStorageDatabase& destination) {
  ASSIGN_OR_RETURN(DomStorageDatabase::Metadata source_metadata,
                   source.ReadAllMetadata());

  // Migrate the `next_map_id` metadata.
  if (source_metadata.next_map_id) {
    DomStorageDatabase::Metadata map_id_metadata;
    map_id_metadata.next_map_id = source_metadata.next_map_id;
    destination.PutMetadata(std::move(map_id_metadata));
  }

  // Migrate each map in `source_metadata`.
  for (DomStorageDatabase::MapMetadata& source_map :
       source_metadata.map_metadata) {
    // Migrate the map's key/value pairs by reading all entries from
    // `source_map`.
    ASSIGN_OR_RETURN((std::map<DomStorageDatabase::Key,
                               DomStorageDatabase::Value> map_entries),
                     source.ReadMapKeyValues(source_map.map_locator.Clone()));

    // Then create a batch update to add all key/value pairs to `destination`.
    DomStorageDatabase::MapBatchUpdate update(source_map.map_locator.Clone());
    for (auto& [key, value] : map_entries) {
      update.entries_to_add.emplace_back(std::move(key), std::move(value));
    }

    // Migrate the map's usage metadata as  part of the batch update.
    bool has_access_metadata = source_map.last_accessed.has_value();
    bool has_write_metadata = source_map.last_modified && source_map.total_size;
    if (has_access_metadata || has_write_metadata) {
      DomStorageDatabase::MapBatchUpdate::Usage usage;
      if (has_access_metadata) {
        usage.SetLastAccessed(*source_map.last_accessed);
      }
      if (has_write_metadata) {
        usage.SetLastModifiedAndTotalSize(*source_map.last_modified,
                                          *source_map.total_size);
      }
      update.map_usage = std::move(usage);
    } else {
      // When no usage metadata exists, write the metadata separately to
      // associate this map's session IDs and storage key with its map ID.
      DomStorageDatabase::Metadata metadata_to_write;
      metadata_to_write.map_metadata.push_back(std::move(source_map));
      DB_RETURN_IF_ERROR(destination.PutMetadata(std::move(metadata_to_write)));
    }

    // Commit the batch update for `destination`, containing the key/value pairs
    // and optional usage metadata.
    std::vector<DomStorageDatabase::MapBatchUpdate> updates;
    updates.push_back(std::move(update));
    DB_RETURN_IF_ERROR(destination.UpdateMaps(std::move(updates)));
  }
  return DbStatus::OK();
}

}  // namespace storage
