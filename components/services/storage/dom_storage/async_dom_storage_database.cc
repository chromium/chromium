// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/async_dom_storage_database.h"

#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"

namespace storage {

namespace {

// Logs the status of a `StatusOr<T>` result to a histogram and returns the
// result unchanged.
template <typename T>
StatusOr<T> RecordExpected(std::string histogram_name,
                           bool in_memory,
                           StatusOr<T> result) {
  if (result.has_value()) {
    DbStatus::OK().Log(histogram_name, in_memory);
  } else {
    result.error().Log(histogram_name, in_memory);
  }
  return result;
}

// Logs a `DbStatus` to a histogram and returns it unchanged.
DbStatus RecordStatus(std::string histogram_name,
                      bool in_memory,
                      DbStatus status) {
  status.Log(histogram_name, in_memory);
  return status;
}

}  // namespace

// static
std::unique_ptr<AsyncDomStorageDatabase> AsyncDomStorageDatabase::Open(
    StorageType storage_type,
    const base::FilePath& database_path,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    StatusCallback callback) {
  std::unique_ptr<AsyncDomStorageDatabase> db(new AsyncDomStorageDatabase(
      storage_type, /*in_memory=*/database_path.empty()));
  DomStorageDatabaseFactory::Open(
      storage_type, database_path, memory_dump_id,
      base::BindOnce(&AsyncDomStorageDatabase::OnDatabaseOpened,
                     db->weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return db;
}

AsyncDomStorageDatabase::AsyncDomStorageDatabase(StorageType storage_type,
                                                 bool in_memory)
    : storage_type_(storage_type), in_memory_(in_memory) {}

AsyncDomStorageDatabase::~AsyncDomStorageDatabase() {
  DCHECK(committers_.empty());
}

void AsyncDomStorageDatabase::ReadMapKeyValues(
    DomStorageDatabase::MapLocator map_locator,
    ReadMapKeyValuesCallback callback) {
  RunDatabaseTask(
      base::BindOnce(
          [](DomStorageDatabase::MapLocator map_locator,
             DomStorageDatabase& db) {
            return db.ReadMapKeyValues(std::move(map_locator));
          },
          std::move(map_locator)),
      base::BindOnce(
          &RecordExpected<
              std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>,
          GetHistogram("ReadMapKeyValues"), in_memory_)
          .Then(std::move(callback)));
}

void AsyncDomStorageDatabase::CloneMap(
    DomStorageDatabase::MapLocator source_map,
    DomStorageDatabase::MapLocator target_map,
    StatusCallback callback) {
  RunDatabaseTask(
      base::BindOnce(
          [](DomStorageDatabase::MapLocator source_map,
             DomStorageDatabase::MapLocator target_map,
             DomStorageDatabase& db) {
            return db.CloneMap(std::move(source_map), std::move(target_map));
          },
          std::move(source_map), std::move(target_map)),
      base::BindOnce(&RecordStatus, GetHistogram("CloneMap"), in_memory_)
          .Then(std::move(callback)));
}

void AsyncDomStorageDatabase::ReadAllMetadata(
    ReadAllMetadataCallback callback) {
  RunDatabaseTask(base::BindOnce([](DomStorageDatabase& db) {
                    return db.ReadAllMetadata();
                  }),
                  base::BindOnce(&RecordExpected<DomStorageDatabase::Metadata>,
                                 GetHistogram("ReadAllMetadata"), in_memory_)
                      .Then(std::move(callback)));
}

void AsyncDomStorageDatabase::PutMetadata(DomStorageDatabase::Metadata metadata,
                                          StatusCallback callback) {
  RunDatabaseTask(
      base::BindOnce(
          [](DomStorageDatabase::Metadata metadata, DomStorageDatabase& db) {
            return db.PutMetadata(std::move(metadata));
          },
          std::move(metadata)),
      base::BindOnce(&RecordStatus, GetHistogram("PutMetadata"), in_memory_)
          .Then(std::move(callback)));
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
      base::BindOnce(&RecordStatus,
                     GetHistogram("DeleteStorageKeysFromSession"), in_memory_)
          .Then(std::move(callback)));
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
      base::BindOnce(&RecordStatus, GetHistogram("DeleteSessions"), in_memory_)
          .Then(std::move(callback)));
}

void AsyncDomStorageDatabase::PurgeOriginsForShutdown(
    std::set<url::Origin> origins) {
  RunDatabaseTask(
      base::BindOnce(
          [](std::set<url::Origin> origins, bool in_memory,
             std::string histogram_name, DomStorageDatabase& db) {
            DbStatus status = db.PurgeOrigins(std::move(origins));
            // We're logging here instead of a future task that might not run
            // because PurgeOrigins() runs at shutdown.
            status.Log(histogram_name, in_memory);
            return status;
          },
          std::move(origins), in_memory_, GetHistogram("PurgeOrigins")),
      base::DoNothing());
}

void AsyncDomStorageDatabase::RewriteDB(StatusCallback callback) {
  RunDatabaseTask(
      base::BindOnce([](DomStorageDatabase& db) { return db.RewriteDB(); }),
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
      base::BindOnce(&RecordStatus, GetHistogram("UpdateMaps"), in_memory_)
          .Then(std::move(run_all)));
}

void AsyncDomStorageDatabase::OnDatabaseOpened(
    StatusCallback callback,
    StatusOr<base::SequenceBound<DomStorageDatabase>> database) {
  database = RecordExpected(GetHistogram("OpenDatabase"), in_memory_,
                            std::move(database));
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

std::string_view AsyncDomStorageDatabase::StorageTypeForHistograms() const {
  switch (storage_type_) {
    case StorageType::kLocalStorage:
      return "Storage.LocalStorage";
    case StorageType::kSessionStorage:
      return "Storage.SessionStorage";
  }
}

std::string AsyncDomStorageDatabase::GetHistogram(
    std::string_view operation) const {
  return base::StrCat({StorageTypeForHistograms(), ".", operation});
}

}  // namespace storage
