// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/async_dom_storage_database.h"

#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace storage {

// static
std::unique_ptr<AsyncDomStorageDatabase> AsyncDomStorageDatabase::OpenDirectory(
    const base::FilePath& directory,
    const std::string& dbname,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    StatusCallback callback) {
  std::unique_ptr<AsyncDomStorageDatabase> db(new AsyncDomStorageDatabase);
  DomStorageDatabaseFactory::OpenDirectory(
      directory, dbname, memory_dump_id, std::move(blocking_task_runner),
      base::BindOnce(&AsyncDomStorageDatabase::OnDatabaseOpened,
                     db->weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return db;
}

// static
std::unique_ptr<AsyncDomStorageDatabase> AsyncDomStorageDatabase::OpenInMemory(
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    const std::string& tracking_name,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    StatusCallback callback) {
  std::unique_ptr<AsyncDomStorageDatabase> db(new AsyncDomStorageDatabase);
  DomStorageDatabaseFactory::OpenInMemory(
      tracking_name, memory_dump_id, std::move(blocking_task_runner),
      base::BindOnce(&AsyncDomStorageDatabase::OnDatabaseOpened,
                     db->weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return db;
}

AsyncDomStorageDatabase::AsyncDomStorageDatabase() = default;

AsyncDomStorageDatabase::~AsyncDomStorageDatabase() {
  DCHECK(committers_.empty());
}

void AsyncDomStorageDatabase::RewriteDB(StatusCallback callback) {
  DCHECK(database_);
  database_.PostTaskWithThisObject(base::BindOnce(
      [](StatusCallback callback,
         scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
         DomStorageDatabase* db) {
        callback_task_runner->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), db->RewriteDB()));
      },
      std::move(callback), base::SequencedTaskRunner::GetCurrentDefault()));
}

void AsyncDomStorageDatabase::RunBatchDatabaseTasks(
    RunBatchTasksContext context,
    std::vector<BatchDatabaseTask> tasks,
    base::OnceCallback<void(DbStatus)> callback) {
  RunDatabaseTask(base::BindOnce(
                      [](RunBatchTasksContext context,
                         std::vector<BatchDatabaseTask> tasks,
                         DomStorageDatabase& db) -> DbStatus {
                        std::unique_ptr<DomStorageBatchOperation> batch =
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

void AsyncDomStorageDatabase::InitiateCommit(Committer* source) {
  std::vector<Commit> commits;
  std::vector<base::OnceCallback<void(DbStatus)>> commit_dones;
  if (base::FeatureList::IsEnabled(kCoalesceStorageAreaCommits)) {
    commits.reserve(committers_.size());
    commit_dones.reserve(committers_.size());
    for (Committer* committer : committers_) {
      std::optional<Commit> commit = committer->CollectCommit();
      if (commit) {
        commits.emplace_back(std::move(*commit));
        commit_dones.emplace_back(committer->GetCommitCompleteCallback());
      }
    }
  } else {
    commits.emplace_back(*source->CollectCommit());
    commit_dones.emplace_back(source->GetCommitCompleteCallback());
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
          [](std::vector<Commit> commits, DomStorageDatabase& db) {
            std::unique_ptr<DomStorageBatchOperation> batch =
                db.CreateBatchOperation();
            for (const Commit& commit : commits) {
              const auto now = base::TimeTicks::Now();
              for (const base::TimeTicks& put_time : commit.timestamps) {
                UMA_HISTOGRAM_LONG_TIMES_100("DOMStorage.CommitMeasuredDelay",
                                             now - put_time);
              }

              if (commit.clear_all_first) {
                batch->DeletePrefixed(commit.prefix);
              }
              for (const auto& entry : commit.entries_to_add) {
                batch->Put(entry.key, entry.value);
              }
              for (const auto& key : commit.keys_to_delete) {
                batch->Delete(key);
              }
              if (commit.copy_to_prefix) {
                batch->CopyPrefixed(commit.prefix,
                                    commit.copy_to_prefix.value());
              }
            }
            return batch->Commit();
          },
          std::move(commits)),
      std::move(run_all));
}

void AsyncDomStorageDatabase::OnDatabaseOpened(
    StatusCallback callback,
    base::SequenceBound<DomStorageDatabase> database,
    DbStatus status) {
  database_ = std::move(database);
  std::vector<BoundDatabaseTask> tasks;
  std::swap(tasks, tasks_to_run_on_open_);
  if (status.ok()) {
    for (auto& task : tasks)
      database_.PostTaskWithThisObject(std::move(task));
  }
  std::move(callback).Run(status);
}

AsyncDomStorageDatabase::Commit::Commit() = default;
AsyncDomStorageDatabase::Commit::~Commit() = default;
AsyncDomStorageDatabase::Commit::Commit(AsyncDomStorageDatabase::Commit&&) =
    default;

}  // namespace storage
