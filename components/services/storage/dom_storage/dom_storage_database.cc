// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_database.h"

#include "base/debug/leak_annotations.h"
#include "base/task/bind_post_task.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/local_storage_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/session_storage_leveldb.h"

namespace storage {
namespace {

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

DomStorageDatabase::MapLocator::MapLocator(std::string source_session_id,
                                           blink::StorageKey source_storage_key)
    : session_id_(source_session_id), storage_key_(source_storage_key) {}

DomStorageDatabase::MapLocator::MapLocator(std::string source_session_id,
                                           blink::StorageKey source_storage_key,
                                           int64_t source_map_id)
    : session_id_(source_session_id),
      storage_key_(source_storage_key),
      map_id_(source_map_id) {}

DomStorageDatabase::MapLocator::~MapLocator() = default;

DomStorageDatabase::MapLocator::MapLocator(MapLocator&&) = default;

DomStorageDatabase::MapLocator& DomStorageDatabase::MapLocator::operator=(
    MapLocator&&) = default;

const blink::StorageKey& DomStorageDatabase::MapLocator::storage_key() const {
  return storage_key_;
}

const std::string& DomStorageDatabase::MapLocator::session_id() const {
  return session_id_;
}

std::optional<int64_t> DomStorageDatabase::MapLocator::map_id() const {
  return map_id_;
}

DomStorageDatabase::Metadata::Metadata() = default;

DomStorageDatabase::Metadata::Metadata(
    std::vector<MapMetadata> source_map_metadata)
    : map_metadata(std::move(source_map_metadata)) {}

DomStorageDatabase::Metadata::~Metadata() = default;

DomStorageDatabase::Metadata::Metadata(Metadata&&) = default;

DomStorageDatabase::Metadata& DomStorageDatabase::Metadata::operator=(
    Metadata&&) = default;

// static
void DomStorageDatabaseFactory::Open(
    StorageType storage_type,
    const base::FilePath& directory,
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OpenCallback callback) {
  switch (storage_type) {
    case StorageType::kLocalStorage:
      return CreateSequenceBoundDomStorageDatabase<LocalStorageLevelDB>(
          std::move(blocking_task_runner), directory, name, memory_dump_id,
          base::BindOnce(&OnDatabaseOpened<LocalStorageLevelDB>,
                         std::move(callback)));

    case StorageType::kSessionStorage:
      return CreateSequenceBoundDomStorageDatabase<SessionStorageLevelDB>(
          std::move(blocking_task_runner), directory, name, memory_dump_id,
          base::BindOnce(&OnDatabaseOpened<SessionStorageLevelDB>,
                         std::move(callback)));
  }
  NOTREACHED();
}

// static
void DomStorageDatabaseFactory::Destroy(
    const base::FilePath& directory,
    const std::string& name,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    base::OnceCallback<void(DbStatus)> callback) {
  DomStorageDatabaseLevelDB::Destroy(
      directory, name, std::move(blocking_task_runner), std::move(callback));
}

template <typename TDatabase>
void DomStorageDatabaseFactory::CreateSequenceBoundDomStorageDatabase(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::FilePath& directory,
    const std::string& name,
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
      .WithArgs(PassKey(), directory, name, memory_dump_id)
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
