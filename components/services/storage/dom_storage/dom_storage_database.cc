// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_database.h"

#include "base/debug/leak_annotations.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/local_storage_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/session_storage_leveldb.h"
#include "components/services/storage/dom_storage/sqlite/local_storage_sqlite.h"
#include "components/services/storage/dom_storage/sqlite/session_storage_sqlite.h"
#include "components/services/storage/dom_storage/sqlite/sqlite_database_utils.h"
#include "components/services/storage/public/cpp/constants.h"

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

// Runs `callback` after casting `TDatabase` to `DomStorageDatabase`.
template <typename TDatabase>
void OnDatabaseOpened(DomStorageDatabaseFactory::OpenCallback callback,
                      StatusOr<base::SequenceBound<TDatabase>> database) {
  if (!database.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(database.error())));
    return;
  }
  base::SequenceBound<DomStorageDatabase> dom_storage_database =
      *std::move(database);
  std::move(callback).Run(std::move(dom_storage_database));
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
void DomStorageDatabaseFactory::Open(
    StorageType storage_type,
    const base::FilePath& database_path,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    OpenCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      GetTaskRunnerForDb(database_path);

  switch (storage_type) {
    case StorageType::kLocalStorage: {
      if (base::FeatureList::IsEnabled(kDomStorageSqlite)) {
        return CreateSequenceBoundDomStorageDatabase<LocalStorageSqlite>(
            std::move(blocking_task_runner), database_path, memory_dump_id,
            base::BindOnce(&OnDatabaseOpened<LocalStorageSqlite>,
                           std::move(callback)));
      }
      return CreateSequenceBoundDomStorageDatabase<LocalStorageLevelDB>(
          std::move(blocking_task_runner), database_path, memory_dump_id,
          base::BindOnce(&OnDatabaseOpened<LocalStorageLevelDB>,
                         std::move(callback)));
    }
    case StorageType::kSessionStorage: {
      if (base::FeatureList::IsEnabled(kDomStorageSqlite)) {
        return CreateSequenceBoundDomStorageDatabase<SessionStorageSqlite>(
            std::move(blocking_task_runner), database_path, memory_dump_id,
            base::BindOnce(&OnDatabaseOpened<SessionStorageSqlite>,
                           std::move(callback)));
      }
      return CreateSequenceBoundDomStorageDatabase<SessionStorageLevelDB>(
          std::move(blocking_task_runner), database_path, memory_dump_id,
          base::BindOnce(&OnDatabaseOpened<SessionStorageLevelDB>,
                         std::move(callback)));
    }
  }
  NOTREACHED();
}

// static
void DomStorageDatabaseFactory::Destroy(const base::FilePath& database_path,
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

template <typename TDatabase>
void DomStorageDatabaseFactory::CreateSequenceBoundDomStorageDatabase(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::FilePath& database_path,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    base::OnceCallback<void(StatusOr<base::SequenceBound<TDatabase>> database)>
        callback) {
  auto database = std::make_unique<base::SequenceBound<TDatabase>>(
      blocking_task_runner, PassKey());

  // Subtle: We bind `database` as an unmanaged pointer during the async opening
  // operation so that it leaks in case the bound callback below never gets a
  // chance to run (because scheduler shutdown happens first).
  //
  // This is because the callback below is posted to
  // SequencedTaskRunner::GetCurrentDefault(), which may not itself be
  // shutdown-blocking; so if shutdown completes before the task runs, the
  // callback below is destroyed along with any of its owned arguments.
  // Meanwhile, SequenceBound destruction posts a task to its bound TaskRunner,
  // which in this case is one which runs shutdown-blocking tasks.
  //
  // The net result of all of this is that if the SequenceBound were an owned
  // argument, it might attempt to post a shutdown-blocking task after shutdown
  // has completed, which is not allowed and will DCHECK. Leaving the object
  // temporarily unmanaged during this window of potential failure avoids such a
  // DCHECK, and if shutdown does not happen during that window, the object's
  // ownership will finally be left to the caller's discretion.
  //
  // See https://crbug.com/1174179.
  auto* database_ptr = database.release();
  ANNOTATE_LEAKING_OBJECT_PTR(database_ptr);

  database_ptr->AsyncCall(&TDatabase::Open)
      .WithArgs(PassKey(), database_path, memory_dump_id)
      .Then(base::BindOnce(
          [](base::SequenceBound<TDatabase>* database_ptr,
             base::OnceCallback<void(
                 StatusOr<base::SequenceBound<TDatabase>> database)> callback,
             DbStatus status) {
            auto database = base::WrapUnique(database_ptr);
            if (status.ok()) {
              std::move(callback).Run(std::move(*database));
            } else {
              std::move(callback).Run(base::unexpected(std::move(status)));
            }
          },
          database_ptr, std::move(callback)));
}

base::PassKey<DomStorageDatabaseFactory>
DomStorageDatabaseFactory::CreatePassKeyForTesting() {
  return base::PassKey<DomStorageDatabaseFactory>();
}

}  // namespace storage
