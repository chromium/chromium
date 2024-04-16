// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/async_dom_storage_database.h"

#include <inttypes.h>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

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
  DomStorageDatabase::OpenDirectory(
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
  DomStorageDatabase::OpenInMemory(
      tracking_name, memory_dump_id, std::move(blocking_task_runner),
      base::BindOnce(&AsyncDomStorageDatabase::OnDatabaseOpened,
                     db->weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return db;
}

AsyncDomStorageDatabase::AsyncDomStorageDatabase() = default;

AsyncDomStorageDatabase::~AsyncDomStorageDatabase() = default;

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
    std::vector<BatchDatabaseTask> tasks,
    base::OnceCallback<void(leveldb::Status)> callback) {
  RunDatabaseTask(base::BindOnce(
                      [](std::vector<BatchDatabaseTask> tasks,
                         const DomStorageDatabase& db) {
                        leveldb::WriteBatch batch;
                        for (auto& task : tasks)
                          std::move(task).Run(&batch, db);
                        return db.Commit(&batch);
                      },
                      std::move(tasks)),
                  std::move(callback));
}

void AsyncDomStorageDatabase::OnDatabaseOpened(
    StatusCallback callback,
    base::SequenceBound<DomStorageDatabase> database,
    leveldb::Status status) {
  database_ = std::move(database);
  std::vector<BoundDatabaseTask> tasks;
  std::swap(tasks, tasks_to_run_on_open_);
  if (status.ok()) {
    for (auto& task : tasks)
      database_.PostTaskWithThisObject(std::move(task));
  }
  std::move(callback).Run(status);
}

}  // namespace storage
