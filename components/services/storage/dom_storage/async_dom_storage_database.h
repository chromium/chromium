// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_ASYNC_DOM_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_ASYNC_DOM_STORAGE_DATABASE_H_

#include <map>
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
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "components/services/storage/dom_storage/session_storage_metadata.h"
#include "storage/common/database/db_status.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace internal {
template <typename TDatabase, typename ResultType>
struct DatabaseTaskTraits;
}  // namespace internal

// Describes the context in which RunBatchDatabaseTasks is called, for
// debugging.
// TODO(crbug.com/40245293): Remove this debug enum once the investigation is
// complete.
enum class RunBatchTasksContext {
  kScavengeUnusedNamespaces,
  kDeleteStorage,
  kCloneNamespace,
  kRegisterNewAreaMap,
  kRegisterShallowClonedNamespace,
  kDoDatabaseDelete,
  kParseNamespaces,
  kTest,
};

// A wrapper around DomStorageDatabase which simplifies usage by queueing
// database operations until the database is opened.
class AsyncDomStorageDatabase {
 public:
  using StatusCallback = base::OnceCallback<void(DbStatus)>;

  AsyncDomStorageDatabase(const AsyncDomStorageDatabase&) = delete;
  AsyncDomStorageDatabase& operator=(const AsyncDomStorageDatabase&) = delete;

  ~AsyncDomStorageDatabase();

  // Creates an `AsyncDomStorageDatabase` then posts a task to open the database
  // on `blocking_task_runner`. Callers may immediately start using the
  // returned `AsyncDomStorageDatabase`. Runs `callback` with the open database
  // result. After failing to open, `AsyncDomStorageDatabase` must be
  // discarded because no database tasks will run.
  //
  // To create an in-memory database, provide an empty `directory`.
  static std::unique_ptr<AsyncDomStorageDatabase> Open(
      StorageType storage_type,
      const base::FilePath& directory,
      const std::string& dbname,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      StatusCallback callback);

  // Represents a batch of changes from a single commit source. There will be
  // zero to one of these per registered Committer when a commit is initiated.
  struct Commit {
    Commit();
    ~Commit();
    Commit(Commit&&);
    Commit(const Commit&) = delete;
    Commit operator=(Commit&) = delete;

    DomStorageDatabase::Key prefix;
    bool clear_all_first;

    std::vector<DomStorageDatabase::KeyValuePair> entries_to_add;
    std::vector<DomStorageDatabase::Key> keys_to_delete;
    std::optional<DomStorageDatabase::Key> copy_to_prefix;
    std::vector<base::TimeTicks> timestamps;
  };

  // An interface that represents a source of commits. Practically speaking,
  // this is a `StorageAreaImpl`.
  class Committer {
   public:
    virtual std::optional<Commit> CollectCommit() = 0;
    virtual base::OnceCallback<void(DbStatus)> GetCommitCompleteCallback() = 0;
  };

  base::SequenceBound<DomStorageDatabase>& database() { return database_; }

  const base::SequenceBound<DomStorageDatabase>& database() const {
    return database_;
  }

  // The functions below use `RunDatabaseTask()` to read and write `database_`
  // through the `DomStorageDatabase` interface. See function comments in
  // `dom_storage_database.h` for more details.
  using ReadAllMetadataCallback =
      base::OnceCallback<void(StatusOr<DomStorageDatabase::Metadata>)>;
  void ReadAllMetadata(ReadAllMetadataCallback callback);
  void PutMetadata(DomStorageDatabase::Metadata metadata,
                   StatusCallback callback);
  void DeleteStorageKeysFromSession(
      std::string session_id,
      std::vector<blink::StorageKey> storage_keys,
      absl::flat_hash_set<int64_t> excluded_cloned_map_ids,
      StatusCallback callback);
  void RewriteDB(StatusCallback callback);

  // TODO(crbug.com/377242771): Temporarily overload `RunDatabaseTask()` to
  // support both `DomStorageDatabase` and `DomStorageDatabaseLevelDB`. After
  // fully migrating to `DomStorageDatabase`, the `DomStorageDatabaseLevelDB`
  // overload can be removed.
  template <typename TDatabase, typename ResultType>
  using DatabaseTask = base::OnceCallback<ResultType(TDatabase&)>;

  template <typename TDatabase, typename ResultType>
  using TaskTraits = internal::DatabaseTaskTraits<TDatabase, ResultType>;

  // Define for `DomStorageDatabase`.
  template <typename ResultType>
  void RunDatabaseTask(DatabaseTask<DomStorageDatabase, ResultType> task,
                       typename TaskTraits<DomStorageDatabase,
                                           ResultType>::CallbackType callback) {
    auto wrapped_task = base::BindOnce(
        [](DatabaseTask<DomStorageDatabase, ResultType> task,
           typename TaskTraits<DomStorageDatabase, ResultType>::CallbackType
               callback,
           scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
           DomStorageDatabase* db) {
          callback_task_runner->PostTask(
              FROM_HERE, TaskTraits<DomStorageDatabase, ResultType>::
                             RunTaskAndBindCallbackToResult(
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

  // TODO(crbug.com/377242771): Delete this function overload and the
  // `TDatabase` template after fully migrating to `DomStorageDatabase`
  // interface.
  //
  // Define for `DomStorageDatabaseLevelDB`.
  template <typename ResultType>
  void RunDatabaseTask(DatabaseTask<DomStorageDatabaseLevelDB, ResultType> task,
                       typename TaskTraits<DomStorageDatabaseLevelDB,
                                           ResultType>::CallbackType callback) {
    auto wrapped_task = base::BindOnce(
        [](DatabaseTask<DomStorageDatabaseLevelDB, ResultType> task,
           typename TaskTraits<DomStorageDatabaseLevelDB,
                               ResultType>::CallbackType callback,
           scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
           DomStorageDatabase* db) {
          callback_task_runner->PostTask(
              FROM_HERE,
              TaskTraits<DomStorageDatabaseLevelDB, ResultType>::
                  RunTaskAndBindCallbackToResult(
                      db->GetLevelDB(), std::move(task), std::move(callback)));
        },
        std::move(task), std::move(callback),
        base::SequencedTaskRunner::GetCurrentDefault());
    if (database_) {
      database_.PostTaskWithThisObject(std::move(wrapped_task));
    } else {
      tasks_to_run_on_open_.push_back(std::move(wrapped_task));
    }
  }

  using BatchDatabaseTask =
      base::OnceCallback<void(DomStorageBatchOperationLevelDB&,
                              const DomStorageDatabaseLevelDB&)>;
  void RunBatchDatabaseTasks(RunBatchTasksContext context,
                             std::vector<BatchDatabaseTask> tasks,
                             base::OnceCallback<void(DbStatus)> callback);

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
  void OnDatabaseOpened(
      StatusCallback callback,
      StatusOr<base::SequenceBound<DomStorageDatabase>> database);

  explicit AsyncDomStorageDatabase();

  base::SequenceBound<DomStorageDatabase> database_;

  using BoundDatabaseTask = base::OnceCallback<void(DomStorageDatabase*)>;
  std::vector<BoundDatabaseTask> tasks_to_run_on_open_;

  std::set<raw_ptr<Committer>> committers_;

  base::WeakPtrFactory<AsyncDomStorageDatabase> weak_ptr_factory_{this};
};

namespace internal {

template <typename TDatabase, typename ResultType>
struct DatabaseTaskTraits {
  using CallbackType = base::OnceCallback<void(ResultType)>;
  static base::OnceClosure RunTaskAndBindCallbackToResult(
      TDatabase& db,
      AsyncDomStorageDatabase::DatabaseTask<TDatabase, ResultType> task,
      CallbackType callback) {
    return base::BindOnce(std::move(callback), std::move(task).Run(db));
  }
};

// This specialization allows database tasks to return tuples while their
// corresponding callback accepts the unpacked values of the tuple as separate
// arguments.
template <typename TDatabase, typename... Args>
struct DatabaseTaskTraits<TDatabase, std::tuple<Args...>> {
  using ResultType = std::tuple<Args...>;
  using CallbackType = base::OnceCallback<void(Args...)>;

  static base::OnceClosure RunTaskAndBindCallbackToResult(
      TDatabase& db,
      AsyncDomStorageDatabase::DatabaseTask<TDatabase, ResultType> task,
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
