// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/shared_storage_manager.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_runner_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/async_shared_storage_database_impl.h"
#include "components/services/storage/shared_storage/shared_storage_options.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/gurl.h"

namespace storage {

SharedStorageManager::SharedStorageManager(
    base::FilePath db_path,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy)
    : SharedStorageManager(std::move(db_path),
                           std::move(special_storage_policy),
                           SharedStorageOptions::Create()) {}

SharedStorageManager::SharedStorageManager(
    base::FilePath db_path,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<SharedStorageOptions> options)
    : in_memory_(db_path.empty()),
      sql_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      db_path_(std::move(db_path)),
      options_(std::move(options)),
      special_storage_policy_(std::move(special_storage_policy)),
      database_(AsyncSharedStorageDatabaseImpl::Create(
          db_path_,
          sql_task_runner_,
          special_storage_policy_,
          options_->GetDatabaseOptions())),
      memory_pressure_listener_(std::make_unique<base::MemoryPressureListener>(
          FROM_HERE,
          base::BindRepeating(&SharedStorageManager::OnMemoryPressure,
                              base::Unretained(this),
                              base::DoNothing()))) {
  timer_.Start(FROM_HERE, options_->stale_origin_purge_initial_interval,
               base::BindOnce(&SharedStorageManager::PurgeStaleOrigins,
                              weak_ptr_factory_.GetWeakPtr()));
}

SharedStorageManager::~SharedStorageManager() {
  RecordShutdownMetrics();
}

void SharedStorageManager::OnMemoryPressure(
    base::OnceCallback<void()> callback,
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  DCHECK(database_);

  // TODO(cammie): Check if MEMORY_PRESSURE_LEVEL_MODERATE should also be
  // ignored.
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  database_->TrimMemory(std::move(callback));
}

void SharedStorageManager::OnOperationResult(OperationResult result) {
  if (result != OperationResult::kSqlError &&
      result != OperationResult::kInitFailure) {
    return;
  }

  if (result == OperationResult::kSqlError) {
    operation_sql_error_count_++;
    return;
  }

  DCHECK_EQ(result, OperationResult::kInitFailure);
  if (in_memory_ && tried_to_recover_from_init_failure_) {
    // We already tried to recover from init failure before---twice if the
    // database was originally file-backed---but are still having problems:
    // there isn't really anything left to try, so just ignore errors.
    return;
  }
  tried_to_recover_from_init_failure_ = true;
  DestroyAndRecreateDatabase();
}

void SharedStorageManager::Get(url::Origin context_origin,
                               std::u16string key,
                               base::OnceCallback<void(GetResult)> callback) {
  DCHECK(database_);
  auto new_callback = base::BindOnce(
      [](base::WeakPtr<SharedStorageManager> manager,
         base::OnceCallback<void(GetResult)> callback, GetResult result) {
        if (manager)
          manager->OnOperationResult(result.result);
        std::move(callback).Run(result);
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  database_->Get(std::move(context_origin), std::move(key),
                 std::move(new_callback));
}

void SharedStorageManager::Set(
    url::Origin context_origin,
    std::u16string key,
    std::u16string value,
    base::OnceCallback<void(OperationResult)> callback,
    SharedStorageDatabase::SetBehavior behavior) {
  DCHECK(database_);
  database_->Set(std::move(context_origin), std::move(key), std::move(value),
                 GetOperationResultCallback(std::move(callback)), behavior);
}

void SharedStorageManager::Append(
    url::Origin context_origin,
    std::u16string key,
    std::u16string value,
    base::OnceCallback<void(OperationResult)> callback) {
  DCHECK(database_);
  database_->Append(std::move(context_origin), std::move(key), std::move(value),
                    GetOperationResultCallback(std::move(callback)));
}

void SharedStorageManager::Delete(
    url::Origin context_origin,
    std::u16string key,
    base::OnceCallback<void(OperationResult)> callback) {
  DCHECK(database_);
  database_->Delete(std::move(context_origin), std::move(key),
                    GetOperationResultCallback(std::move(callback)));
}

void SharedStorageManager::Length(url::Origin context_origin,
                                  base::OnceCallback<void(int)> callback) {
  DCHECK(database_);
  auto new_callback = base::BindOnce(
      [](base::WeakPtr<SharedStorageManager> manager,
         base::OnceCallback<void(int)> callback, int length) {
        OperationResult result = (length != -1) ? OperationResult::kSuccess
                                                : OperationResult::kSqlError;
        if (manager)
          manager->OnOperationResult(result);
        std::move(callback).Run(length);
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  database_->Length(std::move(context_origin), std::move(new_callback));
}

void SharedStorageManager::Keys(
    url::Origin context_origin,
    mojo::PendingRemote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>
        pending_listener,
    base::OnceCallback<void(OperationResult)> callback) {
  DCHECK(database_);
  database_->Keys(std::move(context_origin), std::move(pending_listener),
                  GetOperationResultCallback(std::move(callback)));
}

void SharedStorageManager::Entries(
    url::Origin context_origin,
    mojo::PendingRemote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>
        pending_listener,
    base::OnceCallback<void(OperationResult)> callback) {
  DCHECK(database_);
  database_->Entries(std::move(context_origin), std::move(pending_listener),
                     GetOperationResultCallback(std::move(callback)));
}

void SharedStorageManager::Clear(
    url::Origin context_origin,
    base::OnceCallback<void(OperationResult)> callback) {
  DCHECK(database_);
  database_->Clear(std::move(context_origin),
                   GetOperationResultCallback(std::move(callback)));
}

void SharedStorageManager::PurgeMatchingOrigins(
    OriginMatcherFunction origin_matcher,
    base::Time begin,
    base::Time end,
    base::OnceCallback<void(OperationResult)> callback,
    bool perform_storage_cleanup) {
  DCHECK(database_);
  database_->PurgeMatchingOrigins(
      std::move(origin_matcher), begin, end,
      GetOperationResultCallback(std::move(callback)), perform_storage_cleanup);
}

void SharedStorageManager::FetchOrigins(
    base::OnceCallback<void(std::vector<mojom::StorageUsageInfoPtr>)>
        callback) {
  DCHECK(database_);
  database_->FetchOrigins(std::move(callback));
}

void SharedStorageManager::SetOnDBDestroyedCallbackForTesting(
    base::OnceCallback<void(bool)> callback) {
  on_db_destroyed_callback_for_testing_ = std::move(callback);
}

void SharedStorageManager::OverrideLastUsedTimeForTesting(
    url::Origin context_origin,
    base::Time new_last_used_time,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(database_);
  static_cast<AsyncSharedStorageDatabaseImpl*>(database_.get())
      ->OverrideLastUsedTimeForTesting(  // IN-TEST
          std::move(context_origin), new_last_used_time, std::move(callback));
}

void SharedStorageManager::OverrideSpecialStoragePolicyForTesting(
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy) {
  DCHECK(database_);
  special_storage_policy_ = special_storage_policy;
  static_cast<AsyncSharedStorageDatabaseImpl*>(database_.get())
      ->OverrideSpecialStoragePolicyForTesting(  // IN-TEST
          special_storage_policy);
}

void SharedStorageManager::OverrideDatabaseForTesting(
    std::unique_ptr<AsyncSharedStorageDatabase> override_async_database) {
  database_ = std::move(override_async_database);
}

void SharedStorageManager::DestroyAndRecreateDatabase() {
  bool recreate_in_memory = in_memory_;

  if (!in_memory_ && !tried_to_recreate_on_disk_) {
    tried_to_recreate_on_disk_ = true;
  } else if (!in_memory_ && tried_to_recreate_on_disk_) {
    recreate_in_memory = true;
  }

  // There is already no database.
  if (!database_) {
    OnDatabaseDestroyed(recreate_in_memory, /*success=*/true);
    return;
  }

  // Destroy database, and try again.
  database_->Destroy(base::BindOnce(&SharedStorageManager::OnDatabaseDestroyed,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    recreate_in_memory));
}

void SharedStorageManager::OnDatabaseDestroyed(bool recreate_in_memory,
                                               bool success) {
  // Even if destroying failed we still want to go ahead and try to recreate.
  if (recreate_in_memory) {
    db_path_ = base::FilePath();
    in_memory_ = true;
  }

  database_ = AsyncSharedStorageDatabaseImpl::Create(
      db_path_, sql_task_runner_, special_storage_policy_,
      options_->GetDatabaseOptions());

  if (on_db_destroyed_callback_for_testing_)
    std::move(on_db_destroyed_callback_for_testing_).Run(success);

  // TODO(cammie): Log `success` in a histogram.
}

base::OnceCallback<void(SharedStorageManager::OperationResult)>
SharedStorageManager::GetOperationResultCallback(
    base::OnceCallback<void(OperationResult)> callback) {
  return base::BindOnce(
      [](base::WeakPtr<SharedStorageManager> manager,
         base::OnceCallback<void(OperationResult)> callback,
         OperationResult result) {
        if (manager)
          manager->OnOperationResult(result);
        std::move(callback).Run(result);
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));
}

void SharedStorageManager::PurgeStaleOrigins() {
  DCHECK(!options_->origin_staleness_threshold.is_zero());
  DCHECK(database_);

  database_->PurgeStaleOrigins(
      options_->origin_staleness_threshold,
      base::BindOnce(&SharedStorageManager::OnStaleOriginsPurged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharedStorageManager::OnStaleOriginsPurged(OperationResult result) {
  DCHECK(database_);
  OnOperationResult(result);

  timer_.Start(FROM_HERE, options_->stale_origin_purge_recurring_interval,
               base::BindOnce(&SharedStorageManager::PurgeStaleOrigins,
                              weak_ptr_factory_.GetWeakPtr()));
}

void SharedStorageManager::RecordShutdownMetrics() {
  base::UmaHistogramCounts1000("Storage.SharedStorage.OnShutdown.NumSqlErrors",
                               operation_sql_error_count_);
  base::UmaHistogramBoolean(
      "Storage.SharedStorage.OnShutdown.RecoveryFromInitFailureAttempted",
      tried_to_recover_from_init_failure_);
  base::UmaHistogramBoolean(
      "Storage.SharedStorage.OnShutdown.RecoveryOnDiskAttempted",
      tried_to_recreate_on_disk_);
}

}  // namespace storage
