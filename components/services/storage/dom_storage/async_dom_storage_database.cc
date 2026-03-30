// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/async_dom_storage_database.h"

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"

namespace storage {

namespace {

// Records the duration of an operation to a histogram suffixed with
// ".InMemory" or ".OnDisk".
void RecordDuration(const std::string& histogram_name,
                    bool in_memory,
                    base::TimeTicks start_time) {
  base::UmaHistogramTimes(
      histogram_name + (in_memory ? ".InMemory" : ".OnDisk"),
      base::TimeTicks::Now() - start_time);
}

// Records status and duration histograms for a `StatusOr<T>` result.
template <typename T>
void RecordExpectedAndDuration(const std::string& status_histogram_name,
                               const std::string& duration_histogram_name,
                               bool in_memory,
                               base::TimeTicks start_time,
                               const StatusOr<T>& result) {
  RecordDuration(duration_histogram_name, in_memory, start_time);

  if (result.has_value()) {
    DbStatus::OK().Log(status_histogram_name, in_memory);
  } else {
    result.error().Log(status_histogram_name, in_memory);
  }
}

// Records status and duration histograms for a `DbStatus` result.
void RecordStatusAndDuration(const std::string& status_histogram_name,
                             const std::string& duration_histogram_name,
                             bool in_memory,
                             base::TimeTicks start_time,
                             const DbStatus& status) {
  RecordDuration(duration_histogram_name, in_memory, start_time);
  status.Log(status_histogram_name, in_memory);
}

}  // namespace

// static
std::unique_ptr<AsyncDomStorageDatabase> AsyncDomStorageDatabase::Open(
    StorageType storage_type,
    const base::FilePath& database_path,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    StatusCallback callback) {
  bool is_in_memory = database_path.empty();
  std::unique_ptr<AsyncDomStorageDatabase> instance(
      new AsyncDomStorageDatabase(storage_type, is_in_memory));

  instance->database_ = DomStorageDatabaseFactory::Create(
      storage_type, is_in_memory, GetTaskRunnerForDb(database_path));

  instance->RunTaskOnDbSequenceAndRecordHistograms(
      "OpenDatabase",
      base::BindOnce(
          [](const base::FilePath& database_path,
             const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
                 memory_dump_id,
             DomStorageDatabase* database) {
            return database->Open(database_path, memory_dump_id);
          },
          database_path, memory_dump_id),
      base::BindOnce(&AsyncDomStorageDatabase::OnDatabaseOpened,
                     instance->weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
  return instance;
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
  CHECK(is_database_opened_);

  RunTaskOnDbSequenceAndRecordHistograms<
      std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>(
      "ReadMapKeyValues",
      base::BindOnce(
          [](DomStorageDatabase::MapLocator map_locator,
             DomStorageDatabase* database) {
            return database->ReadMapKeyValues(std::move(map_locator));
          },
          std::move(map_locator)),
      std::move(callback));
}

void AsyncDomStorageDatabase::CloneMap(
    DomStorageDatabase::MapLocator source_map,
    DomStorageDatabase::MapLocator target_map,
    StatusCallback callback) {
  CHECK(is_database_opened_);

  RunTaskOnDbSequenceAndRecordHistograms(
      "CloneMap",
      base::BindOnce(
          [](DomStorageDatabase::MapLocator source_map,
             DomStorageDatabase::MapLocator target_map,
             DomStorageDatabase* database) {
            return database->CloneMap(std::move(source_map),
                                      std::move(target_map));
          },
          std::move(source_map), std::move(target_map)),
      std::move(callback));
}

void AsyncDomStorageDatabase::ReadAllMetadata(
    ReadAllMetadataCallback callback) {
  CHECK(is_database_opened_);

  RunTaskOnDbSequenceAndRecordHistograms<DomStorageDatabase::Metadata>(
      "ReadAllMetadata", base::BindOnce([](DomStorageDatabase* database) {
        return database->ReadAllMetadata();
      }),
      std::move(callback));
}

void AsyncDomStorageDatabase::PutMetadata(DomStorageDatabase::Metadata metadata,
                                          StatusCallback callback) {
  CHECK(is_database_opened_);

  RunTaskOnDbSequenceAndRecordHistograms(
      "PutMetadata",
      base::BindOnce(
          [](DomStorageDatabase::Metadata metadata,
             DomStorageDatabase* database) {
            return database->PutMetadata(std::move(metadata));
          },
          std::move(metadata)),
      std::move(callback));
}

void AsyncDomStorageDatabase::DeleteStorageKeysFromSession(
    std::string session_id,
    std::vector<blink::StorageKey> metadata_to_delete,
    std::vector<DomStorageDatabase::MapLocator> maps_to_delete,
    StatusCallback callback) {
  CHECK(is_database_opened_);

  RunTaskOnDbSequenceAndRecordHistograms(
      "DeleteStorageKeysFromSession",
      base::BindOnce(
          [](std::string session_id,
             std::vector<blink::StorageKey> metadata_to_delete,
             std::vector<DomStorageDatabase::MapLocator> maps_to_delete,
             DomStorageDatabase* database) {
            return database->DeleteStorageKeysFromSession(
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
  CHECK(is_database_opened_);

  RunTaskOnDbSequenceAndRecordHistograms(
      "DeleteSessions",
      base::BindOnce(
          [](std::vector<std::string> session_ids,
             std::vector<DomStorageDatabase::MapLocator> maps_to_delete,
             DomStorageDatabase* database) {
            return database->DeleteSessions(std::move(session_ids),
                                            std::move(maps_to_delete));
          },
          std::move(session_ids), std::move(maps_to_delete)),
      std::move(callback));
}

void AsyncDomStorageDatabase::PurgeOriginsForShutdown(
    std::set<url::Origin> origins) {
  CHECK(is_database_opened_);

  RunTaskOnDbSequenceAndRecordHistograms(
      "PurgeOrigins",
      base::BindOnce(
          [](std::set<url::Origin> origins, DomStorageDatabase* database) {
            return database->PurgeOrigins(std::move(origins));
          },
          std::move(origins)),
      base::DoNothing());
}

void AsyncDomStorageDatabase::CleanUpStaleData(StatusCallback callback) {
  CHECK(is_database_opened_);

  RunTaskOnDbSequenceAndRecordHistograms(
      "CleanUpStaleData", base::BindOnce([](DomStorageDatabase* database) {
        return database->CleanUpStaleData();
      }),
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
  CHECK(is_database_opened_);

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

  RunTaskOnDbSequenceAndRecordHistograms(
      "UpdateMaps",
      base::BindOnce(
          [](std::vector<DomStorageDatabase::MapBatchUpdate> commits,
             DomStorageDatabase* database) {
            return database->UpdateMaps(std::move(commits));
          },
          std::move(commits)),
      std::move(run_all));
}

void AsyncDomStorageDatabase::OnDatabaseOpened(StatusCallback callback,
                                               DbStatus open_status) {
  CHECK(!is_database_opened_);
  is_database_opened_ = open_status.ok();
  std::move(callback).Run(open_status);
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

std::string AsyncDomStorageDatabase::GetDurationHistogram(
    std::string_view operation) const {
  // OpenDatabase uses "OpenDatabase2" for the duration histogram to
  // distinguish it from an earlier obsoleted histogram that also included
  // in-queue time.
  if (operation == "OpenDatabase") {
    return base::StrCat(
        {StorageTypeForHistograms(), ".Duration.OpenDatabase2"});
  }
  return base::StrCat({StorageTypeForHistograms(), ".Duration.", operation});
}

void AsyncDomStorageDatabase::RunTaskOnDbSequenceAndRecordHistograms(
    std::string_view operation,
    base::OnceCallback<DbStatus(DomStorageDatabase*)> db_task,
    StatusCallback callback) {
  database_.PostTaskWithThisObject(base::BindOnce(
      [](base::OnceCallback<DbStatus(DomStorageDatabase*)> db_task,
         std::string status_histogram_name, std::string duration_histogram_name,
         bool in_memory, StatusCallback callback,
         DomStorageDatabase* database) {
        base::TimeTicks start = base::TimeTicks::Now();
        DbStatus status = std::move(db_task).Run(database);
        RecordStatusAndDuration(status_histogram_name, duration_histogram_name,
                                in_memory, start, status);
        std::move(callback).Run(std::move(status));
      },
      std::move(db_task), GetHistogram(operation),
      GetDurationHistogram(operation), in_memory_,
      base::BindPostTaskToCurrentDefault(std::move(callback))));
}

template <typename T>
void AsyncDomStorageDatabase::RunTaskOnDbSequenceAndRecordHistograms(
    std::string_view operation,
    base::OnceCallback<StatusOr<T>(DomStorageDatabase*)> db_task,
    base::OnceCallback<void(StatusOr<T>)> callback) {
  database_.PostTaskWithThisObject(base::BindOnce(
      [](base::OnceCallback<StatusOr<T>(DomStorageDatabase*)> db_task,
         std::string status_histogram_name, std::string duration_histogram_name,
         bool in_memory, base::OnceCallback<void(StatusOr<T>)> callback,
         DomStorageDatabase* database) {
        base::TimeTicks start = base::TimeTicks::Now();
        StatusOr<T> result = std::move(db_task).Run(database);
        RecordExpectedAndDuration(status_histogram_name,
                                  duration_histogram_name, in_memory, start,
                                  result);
        std::move(callback).Run(std::move(result));
      },
      std::move(db_task), GetHistogram(operation),
      GetDurationHistogram(operation), in_memory_,
      base::BindPostTaskToCurrentDefault(std::move(callback))));
}

}  // namespace storage
