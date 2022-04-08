// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_MANAGER_H_
#define COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_MANAGER_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom-forward.h"
#include "components/services/storage/shared_storage/async_shared_storage_database.h"
#include "components/services/storage/shared_storage/public/mojom/shared_storage.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "url/origin.h"

namespace base {
class Time;
}  // namespace base

namespace storage {
class AsyncSharedStorageDatabase;
struct SharedStorageOptions;
class SpecialStoragePolicy;

// Can be accessed via
// `content::StoragePartition::GetOrCreateSharedStorageManager()`.
// Provides the database connection. Wrapper around
// `AsyncSharedStorageDatabase`.
class SharedStorageManager {
 public:
  using InitStatus = SharedStorageDatabase::InitStatus;
  using SetBehavior = SharedStorageDatabase::SetBehavior;
  using OperationResult = SharedStorageDatabase::OperationResult;
  using GetResult = SharedStorageDatabase::GetResult;

  // A callback type to check if a given origin matches a storage policy.
  // Can be passed empty/null where used, which means the origin will always
  // match.
  using OriginMatcherFunction = SharedStorageDatabase::OriginMatcherFunction;

  // If only `db_path` and `special_storage_policy` are passed as parameters,
  // then the members that would have been initialized from `options` are given
  // the values of the corresponding constants in
  // as the default from `SharedStorageOptions`, as drawn from
  // third_party/blink/public/common/features.h
  SharedStorageManager(
      base::FilePath db_path,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);
  SharedStorageManager(
      base::FilePath db_path,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<SharedStorageOptions> options);

  SharedStorageManager(const SharedStorageManager&) = delete;
  SharedStorageManager& operator=(const SharedStorageManager&) = delete;

  virtual ~SharedStorageManager();

  AsyncSharedStorageDatabase* database() { return database_.get(); }

  bool in_memory() const { return in_memory_; }

  bool tried_to_recreate_on_disk_for_testing() const {
    return tried_to_recreate_on_disk_;
  }

  bool tried_to_recover_from_init_failure_for_testing() const {
    return tried_to_recover_from_init_failure_;
  }

  int operation_sql_error_count_for_testing() const {
    return operation_sql_error_count_;
  }

  base::WeakPtr<SharedStorageManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Resets the `database_` pointer.
  void Shutdown();

  // Called when the system is under memory pressure.
  void OnMemoryPressure(
      base::OnceCallback<void()> callback,
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Tallies database errors, watching for consecutive ones. If the threshold
  // `max_allowed_consecutive_operation_errors_` is exceeded, then the database
  // is deleted and recreated in an attempt to recover.
  void OnOperationResult(OperationResult result);

  // Retrieves the `value` for `context_origin` and `key`. `callback` is called
  // with a string `value` if one is found, absl::nullopt otherwise.
  //
  // `key` must be of length at most
  // `SharedStorageDatabaseOptions::max_string_length`, with the burden on the
  // caller to handle errors for strings that exceed this length.
  void Get(url::Origin context_origin,
           std::u16string key,
           base::OnceCallback<void(GetResult)> callback);

  // Sets an entry for `context_origin` and `key` to have `value`.
  // If `behavior` is `kIgnoreIfPresent` and an entry already exists for
  // `context_origin` and `key`, then the database table is not modified.
  // The parameter of `callback` reports whether or not any entry is added.
  //
  // `key` and `value` must be each of length at most
  // `SharedStorageDatabaseOptions::max_string_length`, with the burden on the
  // caller to handle errors for strings that exceed this length. Moreover, if
  // the length retrieved by `Length(context_origin, callback)` equals
  // `SharedStorageDatabaseOptions::max_entries_per_origin_`, `Set()` will fail
  // and the table will not be modified.
  void Set(url::Origin context_origin,
           std::u16string key,
           std::u16string value,
           base::OnceCallback<void(OperationResult)> callback,
           SetBehavior behavior = SetBehavior::kDefault);

  // Appends `value` to the end of the current `value` for `context_origin` and
  // `key`, if `key` exists. If `key` does not exist, creates an entry for `key`
  // with value `value`. The parameter of `callback` reports whether or not any
  // entry is added or modified.
  //
  // `key` and `value` must be each of length at most
  // `SharedStorageDatabaseOptions::max_string_length`, with the burden on the
  // caller to handle errors for strings that exceed this length. Moreover, if
  // the length of the string obtained by concatening the current `script_value`
  // (if one exists) and `value` exceeds
  // `SharedStorageDatabaseOptions::max_string_length`, or if the length
  // retrieved by `Length(context_origin, callback)` equals
  // `SharedStorageDatabaseOptions::max_entries_per_origin_`, `Append()` will
  // fail and the database table will not be modified.
  void Append(url::Origin context_origin,
              std::u16string key,
              std::u16string value,
              base::OnceCallback<void(OperationResult)> callback);

  // Deletes the entry for `context_origin` and `key`. The parameter of
  // `callback` reports whether the deletion is successful.
  //
  // `key` must be of length at most
  // `SharedStorageDatabaseOptions::max_string_length`, with the burden on the
  // caller to handle errors for strings that exceed this length.
  void Delete(url::Origin context_origin,
              std::u16string key,
              base::OnceCallback<void(OperationResult)> callback);

  // The parameter of `callback` reports the number of entries for
  // `context_origin`, 0 if there are none, or -1 on operation failure.
  void Length(url::Origin context_origin,
              base::OnceCallback<void(int)> callback);

  // From a list of all the keys for `context_origin` taken in lexicographic
  // order, send batches of keys to the Shared Storage worklet's async iterator
  // via a remote that consumes `pending_listener`. Calls `callback` with an
  // OperationResult to indicate whether the transaction was successful.
  void Keys(url::Origin context_origin,
            mojo::PendingRemote<
                shared_storage_worklet::mojom::SharedStorageEntriesListener>
                pending_listener,
            base::OnceCallback<void(OperationResult)> callback);

  // From a list of all the key-value pairs for `context_origin` taken in
  // lexicographic order, send batches of key-value pairs to the Shared Storage
  // worklet's async iterator via a remote that consumes `pending_listener`.
  // Calls `callback` with an OperationResult to indicate whether the
  // transaction was successful.
  void Entries(url::Origin context_origin,
               mojo::PendingRemote<
                   shared_storage_worklet::mojom::SharedStorageEntriesListener>
                   pending_listener,
               base::OnceCallback<void(OperationResult)> callback);

  // Clears all entries for `context_origin`. The parameter of `callback`
  // reports whether the operation is successful. Can be called either as part
  // of the Shared Storage API, or else by
  // `browsing_data::SharedStorageHelper::DeleteOrigin()` in order to clear
  // browsing data via the Settings UI.
  //
  // TODO(cammie): Add `browsing_data::SharedStorageHelper` and the rest of the
  // clear browsing data integration.
  void Clear(url::Origin context_origin,
             base::OnceCallback<void(OperationResult)> callback);

  // Clears all origins that match `origin_matcher` run on the owning
  // StoragePartition's `SpecialStoragePolicy` and have `last_used_time` between
  // the times `begin` and `end`. If `perform_storage_cleanup` is true, vacuums
  // the database afterwards. The parameter of `callback` reports whether the
  // transaction was successful. Called by
  // `content::StoragePartitionImpl::DataDeletionHelper::ClearDataOnUIThread()`.
  //
  // Note that `origin_matcher` is accessed on a different sequence than where
  // it was created.
  void PurgeMatchingOrigins(OriginMatcherFunction origin_matcher,
                            base::Time begin,
                            base::Time end,
                            base::OnceCallback<void(OperationResult)> callback,
                            bool perform_storage_cleanup = false);

  // Fetches a vector of `mojom::StorageUsageInfoPtr`, with one
  // `mojom::StorageUsageInfoPtr` for each origin currently using shared storage
  // in this profile. Called by
  // `browsing_data::SharedStorageHelper::StartFetching`.
  void FetchOrigins(
      base::OnceCallback<void(std::vector<mojom::StorageUsageInfoPtr>)>
          callback);

  void SetOnDBDestroyedCallbackForTesting(
      base::OnceCallback<void(bool)> callback);

  void OverrideLastUsedTimeForTesting(url::Origin context_origin,
                                      base::Time new_last_used_time,
                                      base::OnceCallback<void(bool)> callback);

  void OverrideSpecialStoragePolicyForTesting(
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);

  void OverrideDatabaseForTesting(
      std::unique_ptr<AsyncSharedStorageDatabase> override_async_database);

 private:
  void DestroyAndRecreateDatabase();
  void OnDatabaseDestroyed(bool recreate_in_memory, bool success);

  // Returns a new callback that also includes a call to `OnOperationResult()`.
  base::OnceCallback<void(OperationResult)> GetOperationResultCallback(
      base::OnceCallback<void(OperationResult)> callback);

  // Purges the data for any origins that haven't been written to or read from
  // for more than the `origin_staleness_threshold_`.
  void PurgeStaleOrigins();

  // Starts the `timer_` for the next call to `PurgeStaleOrigins()`.
  void OnStaleOriginsPurged(OperationResult result);

  // Records metrics, including how many SQL errors were seen, when destructor
  // is called.
  void RecordShutdownMetrics();

  // Whether the database should be created in-memory only.
  bool in_memory_;

  // Whether an error already caused an attempt to delete and recreate the
  // database on disk.
  bool tried_to_recreate_on_disk_ = false;

  // Whether we have already tried to recover from init failure by throwing the
  // database away and recreating it.
  bool tried_to_recover_from_init_failure_ = false;

  const scoped_refptr<base::SequencedTaskRunner> sql_task_runner_;

  // The file path for the database, if it is disk-based. An empty `db_path_`
  // corresponds to an in-memory database.
  base::FilePath db_path_;

  // Bundled constants that are Finch-configurable.
  std::unique_ptr<SharedStorageOptions> options_;

  // The owning partition's storage policy.
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  // A pointer to the database.
  std::unique_ptr<AsyncSharedStorageDatabase> database_;

  // Timer for purging stale origins.
  base::OneShotTimer timer_;

  // Counts operation errors due to SQL database errors.
  int operation_sql_error_count_ = 0;

  // Listens for the system being under memory pressure.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Callback to be run at the end of `OnDatabaseDestroyed()`.
  base::OnceCallback<void(bool)> on_db_destroyed_callback_for_testing_;

  base::WeakPtrFactory<SharedStorageManager> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_MANAGER_H_
