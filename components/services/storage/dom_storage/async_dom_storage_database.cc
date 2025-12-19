// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/async_dom_storage_database.h"

#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace storage {

// static
scoped_refptr<base::SequencedTaskRunner>
AsyncDomStorageDatabase::GetTaskRunnerForDb(const base::FilePath& directory,
                                            const std::string& dbname) {
  CHECK(!directory.empty());
  return base::ThreadPool::CreateSequencedTaskRunnerForResource(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      directory.AppendASCII(dbname));
}

// static
std::unique_ptr<AsyncDomStorageDatabase> AsyncDomStorageDatabase::Open(
    StorageType storage_type,
    const base::FilePath& directory,
    const std::string& dbname,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    StatusCallback callback) {
  std::unique_ptr<AsyncDomStorageDatabase> db(new AsyncDomStorageDatabase);
  DomStorageDatabaseFactory::Open(
      storage_type, directory, dbname, memory_dump_id,
      // For the in-memory case, blocking shutdown is only important to avoid
      // leaking the SequenceBound on shutdown (and triggering ASAN failures).
      directory.empty() ? base::ThreadPool::CreateSequencedTaskRunner(
                              {base::WithBaseSyncPrimitives(),
                               base::TaskShutdownBehavior::BLOCK_SHUTDOWN})
                        : GetTaskRunnerForDb(directory, dbname),
      base::BindOnce(&AsyncDomStorageDatabase::OnDatabaseOpened,
                     db->weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return db;
}

AsyncDomStorageDatabase::AsyncDomStorageDatabase() = default;

AsyncDomStorageDatabase::~AsyncDomStorageDatabase() {
  DCHECK(committers_.empty());
}

void AsyncDomStorageDatabase::ReadMapKeyValues(
    DomStorageDatabase::MapLocator map_locator,
    ReadMapKeyValuesCallback callback) {
  RunDatabaseTask(base::BindOnce(
                      [](DomStorageDatabase::MapLocator map_locator,
                         DomStorageDatabase& db) {
                        return db.ReadMapKeyValues(std::move(map_locator));
                      },
                      std::move(map_locator)),
                  std::move(callback));
}

void AsyncDomStorageDatabase::CloneMap(
    DomStorageDatabase::MapLocator source_map,
    DomStorageDatabase::MapLocator target_map,
    StatusCallback callback) {
  RunDatabaseTask(base::BindOnce(
                      [](DomStorageDatabase::MapLocator source_map,
                         DomStorageDatabase::MapLocator target_map,
                         DomStorageDatabase& db) {
                        return db.CloneMap(std::move(source_map),
                                           std::move(target_map));
                      },
                      std::move(source_map), std::move(target_map)),
                  std::move(callback));
}

void AsyncDomStorageDatabase::ReadAllMetadata(
    ReadAllMetadataCallback callback) {
  RunDatabaseTask(base::BindOnce([](DomStorageDatabase& db) {
                    return db.ReadAllMetadata();
                  }),
                  std::move(callback));
}

void AsyncDomStorageDatabase::PutMetadata(DomStorageDatabase::Metadata metadata,
                                          StatusCallback callback) {
  RunDatabaseTask(
      base::BindOnce(
          [](DomStorageDatabase::Metadata metadata, DomStorageDatabase& db) {
            return db.PutMetadata(std::move(metadata));
          },
          std::move(metadata)),
      std::move(callback));
}

void AsyncDomStorageDatabase::DeleteStorageKeysFromSession(
    std::string session_id,
    std::vector<blink::StorageKey> metadata_to_delete,
    std::vector<DomStorageDatabase::MapLocator> maps_to_delete,
    StatusCallback callback) {
  RunDatabaseTask(
      base::BindOnce(
          [](std::string session_id,
             std::vector<blink::StorageKey> metadata_to_delete,
             std::vector<DomStorageDatabase::MapLocator> maps_to_delete,
             DomStorageDatabase& db) {
            return db.DeleteStorageKeysFromSession(
                std::move(session_id), std::move(metadata_to_delete),
                std::move(maps_to_delete));
          },
          std::move(session_id), std::move(metadata_to_delete),
          std::move(maps_to_delete)),
      std::move(callback));
}

void AsyncDomStorageDatabase::DeleteSessions(
    std::vector<std::string> session_ids,
    std::vector<DomStorageDatabase::MapLocator> maps_to_delete,
    StatusCallback callback) {
  RunDatabaseTask(
      base::BindOnce(
          [](std::vector<std::string> session_ids,
             std::vector<DomStorageDatabase::MapLocator> maps_to_delete,
             DomStorageDatabase& db) {
            return db.DeleteSessions(std::move(session_ids),
                                     std::move(maps_to_delete));
          },
          std::move(session_ids), std::move(maps_to_delete)),
      std::move(callback));
}

void AsyncDomStorageDatabase::PurgeOriginsForShutdown(
    std::set<url::Origin> origins) {
  RunDatabaseTask(
      base::BindOnce(
          [](std::set<url::Origin> origins, DomStorageDatabase& db) {
            return db.PurgeOrigins(std::move(origins));
          },
          std::move(origins)),
      // Ignore errors since this is called during shutdown.
      base::DoNothing());
}

void AsyncDomStorageDatabase::RewriteDB(StatusCallback callback) {
  RunDatabaseTask(
      base::BindOnce([](DomStorageDatabase& db) { return db.RewriteDB(); }),
      std::move(callback));
}

void AsyncDomStorageDatabase::RunBatchDatabaseTasks(
    RunBatchTasksContext context,
    std::vector<BatchDatabaseTask> tasks,
    base::OnceCallback<void(DbStatus)> callback) {
  RunDatabaseTask(base::BindOnce(
                      [](RunBatchTasksContext context,
                         std::vector<BatchDatabaseTask> tasks,
                         DomStorageDatabaseLevelDB& db) -> DbStatus {
                        std::unique_ptr<DomStorageBatchOperationLevelDB> batch =
                            db.CreateBatchOperation();
                        // TODO(crbug.com/40245293): Remove this after debugging
                        // is complete.
                        base::debug::Alias(&context);
                        size_t batch_task_count = tasks.size();
                        size_t iteration_count = 0;
                        size_t current_batch_size =
                            batch->ApproximateSizeForMetrics();
                        base::debug::Alias(&batch_task_count);
                        base::debug::Alias(&iteration_count);
                        base::debug::Alias(&current_batch_size);
                        for (auto& task : tasks) {
                          iteration_count++;
                          std::move(task).Run(*batch, db);
                          size_t growth = batch->ApproximateSizeForMetrics() -
                                          current_batch_size;
                          base::UmaHistogramCustomCounts(
                              "Storage.DomStorage."
                              "BatchTaskGrowthSizeBytes2",
                              growth, 1, 100 * 1024 * 1024, 50);
                          const size_t kTargetBatchSizesMB[] = {20, 100, 500};
                          for (size_t batch_size_mb : kTargetBatchSizesMB) {
                            size_t target_batch_size =
                                batch_size_mb * 1024 * 1024;
                            if (current_batch_size < target_batch_size &&
                                batch->ApproximateSizeForMetrics() >=
                                    target_batch_size) {
                              base::UmaHistogramCounts10000(
                                  base::StringPrintf("Storage.DomStorage."
                                                     "IterationsToReach%zuMB2",
                                                     batch_size_mb),
                                  iteration_count);
                            }
                          }
                          current_batch_size =
                              batch->ApproximateSizeForMetrics();
                        }
                        return batch->Commit();
                      },
                      context, std::move(tasks)),
                  std::move(callback));
}

void AsyncDomStorageDatabase::AddCommitter(Committer* source) {
  auto iter = committers_.insert(source);
  DCHECK(iter.second);
}

void AsyncDomStorageDatabase::RemoveCommitter(Committer* source) {
  size_t erased = committers_.erase(source);
  DCHECK(erased);
}

void AsyncDomStorageDatabase::InitiateCommit() {
  std::vector<DomStorageDatabase::MapBatchUpdate> commits;
  std::vector<base::OnceCallback<void(DbStatus)>> commit_dones;
  commit_dones.reserve(committers_.size());
  for (Committer* committer : committers_) {
    std::optional<DomStorageDatabase::MapBatchUpdate> commit =
        committer->CollectCommit();
    if (commit) {
      commits.push_back(*std::move(commit));
      commit_dones.emplace_back(committer->GetCommitCompleteCallback());
    }
  }

  auto run_all = base::BindOnce(
      [](std::vector<base::OnceCallback<void(DbStatus)>> callbacks,
         DbStatus status) {
        for (auto& callback : callbacks) {
          std::move(callback).Run(status);
        }
      },
      std::move(commit_dones));

  RunDatabaseTask(
      base::BindOnce(
          [](std::vector<DomStorageDatabase::MapBatchUpdate> commits,
             DomStorageDatabase& db) {
            return db.UpdateMaps(std::move(commits));
          },
          std::move(commits)),
      std::move(run_all));
}

void AsyncDomStorageDatabase::OnDatabaseOpened(
    StatusCallback callback,
    StatusOr<base::SequenceBound<DomStorageDatabase>> database) {
  if (!database.has_value()) {
    std::move(callback).Run(std::move(database.error()));
    return;
  }

  database_ = *std::move(database);

  std::vector<BoundDatabaseTask> tasks;
  std::swap(tasks, tasks_to_run_on_open_);

  for (auto& task : tasks) {
    database_.PostTaskWithThisObject(std::move(task));
  }
  std::move(callback).Run(DbStatus::OK());
}

}  // namespace storage
