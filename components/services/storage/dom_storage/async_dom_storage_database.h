// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_ASYNC_DOM_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_ASYNC_DOM_STORAGE_DATABASE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/features.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/session_storage_metadata.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace internal {
template <typename ResultType>
struct DatabaseTaskTraits;
}  // namespace internal

// A wrapper around DomStorageDatabase which simplifies usage by queueing
// database operations until the database is opened.
class AsyncDomStorageDatabase {
 public:
  using StatusCallback = base::OnceCallback<void(DbStatus)>;

  AsyncDomStorageDatabase(const AsyncDomStorageDatabase&) = delete;
  AsyncDomStorageDatabase& operator=(const AsyncDomStorageDatabase&) = delete;

  ~AsyncDomStorageDatabase();

  // Creates an `AsyncDomStorageDatabase` then asynchronously opens the
  // database. Callers may immediately start using the returned
  // `AsyncDomStorageDatabase`. Runs `callback` with the open database result.
  // After failing to open, `AsyncDomStorageDatabase` must be discarded because
  // no database tasks will run.
  //
  // To create an in-memory database, provide an empty `database_path`.
  static std::unique_ptr<AsyncDomStorageDatabase> Open(
      StorageType storage_type,
      const base::FilePath& database_path,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      StatusCallback callback);

  // An interface that represents a source of commits. Practically speaking,
  // this is a `StorageAreaImpl`.
  class Committer {
   public:
    virtual std::optional<DomStorageDatabase::MapBatchUpdate>
    CollectCommit() = 0;
    virtual base::OnceCallback<void(DbStatus)> GetCommitCompleteCallback() = 0;
  };

  base::SequenceBound<DomStorageDatabase>& database() { return database_; }

  // The functions below use `RunDatabaseTask()` to read and write `database_`
  // through the `DomStorageDatabase` interface. See function comments in
  // `dom_storage_database.h` for more details.
  using ReadMapKeyValuesCallback = base::OnceCallback<void(
      StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>)>;
  void ReadMapKeyValues(DomStorageDatabase::MapLocator map_locator,
                        ReadMapKeyValuesCallback callback);
  void CloneMap(DomStorageDatabase::MapLocator source_map,
                DomStorageDatabase::MapLocator target_map,
                StatusCallback callback);

  using ReadAllMetadataCallback =
      base::OnceCallback<void(StatusOr<DomStorageDatabase::Metadata>)>;
  void ReadAllMetadata(ReadAllMetadataCallback callback);

  void PutMetadata(DomStorageDatabase::Metadata metadata,
                   StatusCallback callback);
  void DeleteStorageKeysFromSession(
      std::string session_id,
      std::vector<blink::StorageKey> metadata_to_delete,
      std::vector<DomStorageDatabase::MapLocator> maps_to_delete,
      StatusCallback callback);
  void DeleteSessions(
      std::vector<std::string> session_ids,
      std::vector<DomStorageDatabase::MapLocator> maps_to_delete,
      StatusCallback callback);
  void PurgeOriginsForShutdown(std::set<url::Origin> origins);
  void RewriteDB(StatusCallback callback);

  template <typename ResultType>
  using DatabaseTask = base::OnceCallback<ResultType(DomStorageDatabase&)>;

  template <typename ResultType>
  using TaskTraits = internal::DatabaseTaskTraits<ResultType>;

  // Define for `DomStorageDatabase`.
  template <typename ResultType>
  void RunDatabaseTask(DatabaseTask<ResultType> task,
                       typename TaskTraits<ResultType>::CallbackType callback) {
    auto wrapped_task = base::BindOnce(
        [](DatabaseTask<ResultType> task,
           typename TaskTraits<ResultType>::CallbackType callback,
           scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
           DomStorageDatabase* db) {
          callback_task_runner->PostTask(
              FROM_HERE, TaskTraits<ResultType>::RunTaskAndBindCallbackToResult(
                             *db, std::move(task), std::move(callback)));
        },
        std::move(task), std::move(callback),
        base::SequencedTaskRunner::GetCurrentDefault());
    if (database_) {
      database_.PostTaskWithThisObject(std::move(wrapped_task));
    } else {
      tasks_to_run_on_open_.push_back(std::move(wrapped_task));
    }
  }

  // Registers or unregisters `source` such that its commits will be batched
  // with other registered committers.
  void AddCommitter(Committer* source);
  void RemoveCommitter(Committer* source);

  // To be called by a committer when it has data that should be committed
  // without delay. Persists the list of pending `Commit` batches from
  // `committers_` using `RunDatabaseTask()`. After the database task, runs the
  // completed callback for each `Committer` that provided a `Commit`.
  void InitiateCommit();

 private:
  AsyncDomStorageDatabase(StorageType storage_type, bool in_memory);

  void OnDatabaseOpened(
      StatusCallback callback,
      StatusOr<base::SequenceBound<DomStorageDatabase>> database);

  std::string_view StorageTypeForHistograms() const;
  std::string GetHistogram(std::string_view operation) const;

  base::SequenceBound<DomStorageDatabase> database_;

  using BoundDatabaseTask = base::OnceCallback<void(DomStorageDatabase*)>;
  std::vector<BoundDatabaseTask> tasks_to_run_on_open_;

  std::set<raw_ptr<Committer>> committers_;

  const StorageType storage_type_;
  const bool in_memory_;

  base::WeakPtrFactory<AsyncDomStorageDatabase> weak_ptr_factory_{this};
};

namespace internal {

template <typename ResultType>
struct DatabaseTaskTraits {
  using CallbackType = base::OnceCallback<void(ResultType)>;
  static base::OnceClosure RunTaskAndBindCallbackToResult(
      DomStorageDatabase& db,
      AsyncDomStorageDatabase::DatabaseTask<ResultType> task,
      CallbackType callback) {
    return base::BindOnce(std::move(callback), std::move(task).Run(db));
  }
};

// This specialization allows database tasks to return tuples while their
// corresponding callback accepts the unpacked values of the tuple as separate
// arguments.
template <typename... Args>
struct DatabaseTaskTraits<std::tuple<Args...>> {
  using ResultType = std::tuple<Args...>;
  using CallbackType = base::OnceCallback<void(Args...)>;

  static base::OnceClosure RunTaskAndBindCallbackToResult(
      DomStorageDatabase& db,
      AsyncDomStorageDatabase::DatabaseTask<ResultType> task,
      CallbackType callback) {
    return BindTupleAsArgs(
        std::move(callback), std::move(task).Run(db),
        std::make_index_sequence<std::tuple_size<ResultType>::value>{});
  }

 private:
  template <typename Tuple, size_t... Indices>
  static base::OnceClosure BindTupleAsArgs(CallbackType callback,
                                           Tuple tuple,
                                           std::index_sequence<Indices...>) {
    return base::BindOnce(std::move(callback),
                          std::move(std::get<Indices>(tuple))...);
  }
};

}  // namespace internal

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_ASYNC_DOM_STORAGE_DATABASE_H_
