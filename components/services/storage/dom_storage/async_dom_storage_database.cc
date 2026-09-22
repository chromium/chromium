// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/async_dom_storage_database.h"

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "components/services/storage/dom_storage/dom_storage_histogram_helper.h"
#include "components/services/storage/dom_storage/features.h"

namespace storage {

namespace {

// Records the duration of an operation to a histogram with the suffix
// corresponding to `metrics_type`.
void RecordDuration(const std::string& histogram_name,
                    DatabaseMetricsType metrics_type,
                    base::TimeTicks start_time) {
  base::UmaHistogramTimes(
      base::StrCat({histogram_name, GetHistogramSuffix(metrics_type)}),
      base::TimeTicks::Now() - start_time);
}

// Records status and duration histograms for a `StatusOr<T>` result.
template <typename T>
void RecordExpectedAndDuration(const std::string& status_histogram_name,
                               const std::string& duration_histogram_name,
                               DatabaseMetricsType metrics_type,
                               base::TimeTicks start_time,
                               const StatusOr<T>& result) {
  RecordDuration(duration_histogram_name, metrics_type, start_time);

  if (result.has_value()) {
    DbStatus::OK().Log(status_histogram_name, metrics_type);
  } else {
    result.error().Log(status_histogram_name, metrics_type);
  }
}

// Records status and duration histograms for a `DbStatus` result.
void RecordStatusAndDuration(const std::string& status_histogram_name,
                             const std::string& duration_histogram_name,
                             DatabaseMetricsType metrics_type,
                             base::TimeTicks start_time,
                             const DbStatus& status) {
  RecordDuration(duration_histogram_name, metrics_type, start_time);
  status.Log(status_histogram_name, metrics_type);
}

}  // namespace

// static
std::unique_ptr<AsyncDomStorageDatabase> AsyncDomStorageDatabase::Open(
    StorageType storage_type,
    const base::FilePath& dir_to_open,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    const base::FilePath& dir_to_destroy,
    OpenCallback callback) {
  std::unique_ptr<AsyncDomStorageDatabase> instance(
      new AsyncDomStorageDatabase(storage_type, dir_to_open, memory_dump_id));

  DomStorageDatabaseFactory::Open(
      storage_type, dir_to_open, memory_dump_id, dir_to_destroy,
      base::BindOnce(&AsyncDomStorageDatabase::OnDatabaseOpened,
                     instance->weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));

  return instance;
}

AsyncDomStorageDatabase::AsyncDomStorageDatabase(
    StorageType storage_type,
    const base::FilePath& dir_to_open,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id)
    : storage_type_(storage_type),
      dir_to_open_(dir_to_open),
      memory_dump_id_(memory_dump_id) {}

AsyncDomStorageDatabase::~AsyncDomStorageDatabase() {
  DCHECK(committers_.empty());
}

void AsyncDomStorageDatabase::ReadMapKeyValues(
    DomStorageDatabase::MapLocator map_locator,
    ReadMapKeyValuesCallback callback) {
  CHECK(is_database_opened_);

  if (migration_state_ == MigrationState::kMigrating) {
    tasks_blocked_on_migration_.push_back(
        base::BindOnce(&AsyncDomStorageDatabase::ReadMapKeyValues,
                       weak_ptr_factory_.GetWeakPtr(), std::move(map_locator),
                       std::move(callback)));
    return;
  }

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

  if (migration_state_ == MigrationState::kMigrating) {
    tasks_blocked_on_migration_.push_back(base::BindOnce(
        &AsyncDomStorageDatabase::CloneMap, weak_ptr_factory_.GetWeakPtr(),
        std::move(source_map), std::move(target_map), std::move(callback)));
    return;
  }

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

  if (migration_state_ == MigrationState::kMigrating) {
    tasks_blocked_on_migration_.push_back(
        base::BindOnce(&AsyncDomStorageDatabase::ReadAllMetadata,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  RunTaskOnDbSequenceAndRecordHistograms<DomStorageDatabase::Metadata>(
      "ReadAllMetadata", base::BindOnce([](DomStorageDatabase* database) {
        return database->ReadAllMetadata();
      }),
      std::move(callback));
}

void AsyncDomStorageDatabase::PutMetadata(DomStorageDatabase::Metadata metadata,
                                          StatusCallback callback) {
  CHECK(is_database_opened_);

  if (migration_state_ == MigrationState::kMigrating) {
    tasks_blocked_on_migration_.push_back(base::BindOnce(
        &AsyncDomStorageDatabase::PutMetadata, weak_ptr_factory_.GetWeakPtr(),
        std::move(metadata), std::move(callback)));
    return;
  }

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

  if (migration_state_ == MigrationState::kMigrating) {
    tasks_blocked_on_migration_.push_back(
        base::BindOnce(&AsyncDomStorageDatabase::DeleteStorageKeysFromSession,
                       weak_ptr_factory_.GetWeakPtr(), std::move(session_id),
                       std::move(metadata_to_delete), std::move(maps_to_delete),
                       std::move(callback)));
    return;
  }

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

  if (migration_state_ == MigrationState::kMigrating) {
    tasks_blocked_on_migration_.push_back(
        base::BindOnce(&AsyncDomStorageDatabase::DeleteSessions,
                       weak_ptr_factory_.GetWeakPtr(), std::move(session_ids),
                       std::move(maps_to_delete), std::move(callback)));
    return;
  }

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

  if (migration_state_ == MigrationState::kMigrating) {
    tasks_blocked_on_migration_.push_back(
        base::BindOnce(&AsyncDomStorageDatabase::PurgeOriginsForShutdown,
                       weak_ptr_factory_.GetWeakPtr(), std::move(origins)));
    return;
  }

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

  if (migration_state_ == MigrationState::kMigrating) {
    tasks_blocked_on_migration_.push_back(
        base::BindOnce(&AsyncDomStorageDatabase::CleanUpStaleData,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

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

  if (migration_state_ == MigrationState::kMigrating) {
    tasks_blocked_on_migration_.push_back(
        base::BindOnce(&AsyncDomStorageDatabase::InitiateCommit,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

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

void AsyncDomStorageDatabase::OnDatabaseOpened(
    OpenCallback callback,
    DomStorageDatabaseFactory::OpenResult result) {
  CHECK_EQ(migration_state_, MigrationState::kInactive);
  CHECK(!is_database_opened_);

  metrics_type_ = result.metrics_type;
  is_sqlite_ = result.is_sqlite;

  is_database_opened_ = result.open_status.ok();
  if (is_database_opened_) {
    database_ = result.TakeDatabase();
    CHECK(database_);
    if (!dir_to_open_.empty() && !is_sqlite_ &&
        base::FeatureList::IsEnabled(kDomStorageSqliteMigration)) {
      migration_state_ = MigrationState::kAwaitingIdle;
      migration_timer_.Start(
          FROM_HERE, kDomStorageSqliteMigrationInactivityTimeoutParam.Get(),
          base::BindRepeating(&AsyncDomStorageDatabase::StartMigration,
                              weak_ptr_factory_.GetWeakPtr()));
    }
  }

  std::move(callback).Run(OpenOutcome{std::move(result.open_status),
                                      std::move(result.destroy_outcome)});
}

void AsyncDomStorageDatabase::ResetMigrationTimer() {
  if (migration_state_ != MigrationState::kAwaitingIdle) {
    return;
  }
  migration_timer_.Reset();
}

void AsyncDomStorageDatabase::StartMigration() {
  CHECK(is_database_opened_);
  CHECK_EQ(migration_state_, MigrationState::kAwaitingIdle);

  for (Committer* committer : committers_) {
    if (committer->HasPendingCommit()) {
      InitiateCommit();
      return;
    }
  }

  migration_state_ = MigrationState::kMigrating;

  database_.PostTaskWithThisObject(
      base::BindOnce(&DomStorageDatabaseFactory::Migrate, storage_type_,
                     dir_to_open_, memory_dump_id_,
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &AsyncDomStorageDatabase::OnMigrationFinished,
                         weak_ptr_factory_.GetWeakPtr()))));
}

void AsyncDomStorageDatabase::OnMigrationFinished(
    DomStorageDatabaseFactory::OpenResult result) {
  CHECK_EQ(migration_state_, MigrationState::kMigrating);
  CHECK(is_database_opened_);

  if (result.open_status.ok()) {
    CHECK(result.is_sqlite);

    metrics_type_ = result.metrics_type;
    is_sqlite_ = true;

    // Replaces LevelDB, which the migration deleted.
    database_ = result.TakeDatabase();
    migration_state_ = MigrationState::kCompleted;
  } else {
    // The migration failed, leaving the LevelDB intact, so keep using it.
    migration_state_ = MigrationState::kAborted;
  }

  // Run any operations that were blocked while migrating.
  std::vector<base::OnceClosure> tasks_to_run =
      std::move(tasks_blocked_on_migration_);
  for (auto& task : tasks_to_run) {
    std::move(task).Run();
  }
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
  return base::StrCat({StorageTypeForHistograms(), ".Duration.", operation});
}

void AsyncDomStorageDatabase::RunTaskOnDbSequenceAndRecordHistograms(
    std::string_view operation,
    base::OnceCallback<DbStatus(DomStorageDatabase*)> db_task,
    StatusCallback callback) {
  ResetMigrationTimer();

  database_.PostTaskWithThisObject(base::BindOnce(
      [](base::OnceCallback<DbStatus(DomStorageDatabase*)> db_task,
         std::string status_histogram_name, std::string duration_histogram_name,
         DatabaseMetricsType metrics_type, StatusCallback callback,
         DomStorageDatabase* database) {
        base::TimeTicks start = base::TimeTicks::Now();
        DbStatus status = std::move(db_task).Run(database);
        RecordStatusAndDuration(status_histogram_name, duration_histogram_name,
                                metrics_type, start, status);
        std::move(callback).Run(std::move(status));
      },
      std::move(db_task), GetHistogram(operation),
      GetDurationHistogram(operation), metrics_type_,
      base::BindPostTaskToCurrentDefault(std::move(callback))));
}

template <typename T>
void AsyncDomStorageDatabase::RunTaskOnDbSequenceAndRecordHistograms(
    std::string_view operation,
    base::OnceCallback<StatusOr<T>(DomStorageDatabase*)> db_task,
    base::OnceCallback<void(StatusOr<T>)> callback) {
  ResetMigrationTimer();

  database_.PostTaskWithThisObject(base::BindOnce(
      [](base::OnceCallback<StatusOr<T>(DomStorageDatabase*)> db_task,
         std::string status_histogram_name, std::string duration_histogram_name,
         DatabaseMetricsType metrics_type,
         base::OnceCallback<void(StatusOr<T>)> callback,
         DomStorageDatabase* database) {
        base::TimeTicks start = base::TimeTicks::Now();
        StatusOr<T> result = std::move(db_task).Run(database);
        RecordExpectedAndDuration(status_histogram_name,
                                  duration_histogram_name, metrics_type, start,
                                  result);
        std::move(callback).Run(std::move(result));
      },
      std::move(db_task), GetHistogram(operation),
      GetDurationHistogram(operation), metrics_type_,
      base::BindPostTaskToCurrentDefault(std::move(callback))));
}

}  // namespace storage
