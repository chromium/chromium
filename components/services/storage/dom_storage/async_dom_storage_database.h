// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_ASYNC_DOM_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_ASYNC_DOM_STORAGE_DATABASE_H_

#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include "base/features.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/unguessable_token.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/features.h"
#include "third_party/leveldatabase/src/include/leveldb/cache.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace storage {

namespace internal {
template <typename ResultType>
struct DatabaseTaskTraits;
}  // namespace internal

// A wrapper around DomStorageDatabase which simplifies usage by queueing
// database operations until the database is opened.
class AsyncDomStorageDatabase {
 public:
  using StatusCallback = base::OnceCallback<void(leveldb::Status)>;

  AsyncDomStorageDatabase(const AsyncDomStorageDatabase&) = delete;
  AsyncDomStorageDatabase& operator=(const AsyncDomStorageDatabase&) = delete;

  ~AsyncDomStorageDatabase();

  static std::unique_ptr<AsyncDomStorageDatabase> OpenDirectory(
      const base::FilePath& directory,
      const std::string& dbname,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      StatusCallback callback);

  static std::unique_ptr<AsyncDomStorageDatabase> OpenInMemory(
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      const std::string& tracking_name,
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

    // For metrics.
    size_t data_size;
  };

  // An interface that represents a source of commits. Practically speaking,
  // this is a `StorageAreaImpl`.
  class Committer {
   public:
    virtual std::optional<Commit> CollectCommit() = 0;
    virtual base::OnceCallback<void(leveldb::Status)>
    GetCommitCompleteCallback() = 0;
  };

  base::SequenceBound<DomStorageDatabase>& database() { return database_; }
  const base::SequenceBound<DomStorageDatabase>& database() const {
    return database_;
  }

  void RewriteDB(StatusCallback callback);

  template <typename ResultType>
  using DatabaseTask =
      base::OnceCallback<ResultType(const DomStorageDatabase&)>;

  template <typename ResultType>
  using TaskTraits = internal::DatabaseTaskTraits<ResultType>;

  template <typename ResultType>
  void RunDatabaseTask(DatabaseTask<ResultType> task,
                       typename TaskTraits<ResultType>::CallbackType callback) {
    auto wrapped_task = base::BindOnce(
        [](DatabaseTask<ResultType> task,
           typename TaskTraits<ResultType>::CallbackType callback,
           scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
           const DomStorageDatabase& db) {
          callback_task_runner->PostTask(
              FROM_HERE, TaskTraits<ResultType>::RunTaskAndBindCallbackToResult(
                             db, std::move(task), std::move(callback)));
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
      base::OnceCallback<void(leveldb::WriteBatch*, const DomStorageDatabase&)>;
  void RunBatchDatabaseTasks(
      std::vector<BatchDatabaseTask> tasks,
      base::OnceCallback<void(leveldb::Status)> callback);

  // Registers or unregisters `source` such that its commits will be batched
  // with other registered committers.
  void AddCommitter(Committer* source);
  void RemoveCommitter(Committer* source);

  // To be called by a committer when it has data that should be committed
  // without delay. TODO(crbug.com/340200017): the parameter only exists to
  // support the legacy behavior of distinct commits per storage area, and
  // should be removed when kCoalesceStorageAreaCommits is enabled by default.
  void InitiateCommit(Committer* source);

 private:
  void OnDatabaseOpened(StatusCallback callback,
                        base::SequenceBound<DomStorageDatabase> database,
                        leveldb::Status status);

  explicit AsyncDomStorageDatabase();

  base::SequenceBound<DomStorageDatabase> database_;

  using BoundDatabaseTask = base::OnceCallback<void(const DomStorageDatabase&)>;
  std::vector<BoundDatabaseTask> tasks_to_run_on_open_;
  std::set<raw_ptr<Committer>> committers_;

  base::WeakPtrFactory<AsyncDomStorageDatabase> weak_ptr_factory_{this};
};

namespace internal {

template <typename ResultType>
struct DatabaseTaskTraits {
  using CallbackType = base::OnceCallback<void(ResultType)>;
  static base::OnceClosure RunTaskAndBindCallbackToResult(
      const DomStorageDatabase& db,
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
      const DomStorageDatabase& db,
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
