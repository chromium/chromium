// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_ASYNC_DOM_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_ASYNC_DOM_STORAGE_DATABASE_H_

#include <memory>
#include <tuple>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
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

  ~AsyncDomStorageDatabase();

  static std::unique_ptr<AsyncDomStorageDatabase> OpenDirectory(
      const leveldb_env::Options& options,
      const base::FilePath& directory,
      const std::string& dbname,
      const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      StatusCallback callback);

  static std::unique_ptr<AsyncDomStorageDatabase> OpenInMemory(
      const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      const std::string& tracking_name,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      StatusCallback callback);

  base::SequenceBound<DomStorageDatabase>& database() { return database_; }
  const base::SequenceBound<DomStorageDatabase>& database() const {
    return database_;
  }

  void Put(const std::vector<uint8_t>& key,
           const std::vector<uint8_t>& value,
           StatusCallback callback);

  void Delete(const std::vector<uint8_t>& key, StatusCallback callback);

  void DeletePrefixed(const std::vector<uint8_t>& key_prefix,
                      StatusCallback callback);

  void RewriteDB(StatusCallback callback);

  using GetCallback = base::OnceCallback<void(leveldb::Status status,
                                              const std::vector<uint8_t>&)>;
  void Get(const std::vector<uint8_t>& key, GetCallback callback);

  void CopyPrefixed(const std::vector<uint8_t>& source_key_prefix,
                    const std::vector<uint8_t>& destination_key_prefix,
                    StatusCallback callback);

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
        base::SequencedTaskRunnerHandle::Get());
    if (database_) {
      database_.PostTaskWithThisObject(FROM_HERE, std::move(wrapped_task));
    } else {
      tasks_to_run_on_open_.push_back(std::move(wrapped_task));
    }
  }

  using BatchDatabaseTask =
      base::OnceCallback<void(leveldb::WriteBatch*, const DomStorageDatabase&)>;
  void RunBatchDatabaseTasks(
      std::vector<BatchDatabaseTask> tasks,
      base::OnceCallback<void(leveldb::Status)> callback);

 private:
  void OnDatabaseOpened(StatusCallback callback,
                        base::SequenceBound<DomStorageDatabase> database,
                        leveldb::Status status);

  explicit AsyncDomStorageDatabase();

  base::SequenceBound<DomStorageDatabase> database_;

  using BoundDatabaseTask = base::OnceCallback<void(const DomStorageDatabase&)>;
  std::vector<BoundDatabaseTask> tasks_to_run_on_open_;

  base::WeakPtrFactory<AsyncDomStorageDatabase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AsyncDomStorageDatabase);
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
