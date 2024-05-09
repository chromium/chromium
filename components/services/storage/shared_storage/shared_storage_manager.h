// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_MANAGER_H_
#define COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_MANAGER_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom-forward.h"
#include "components/services/storage/shared_storage/async_shared_storage_database.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "url/origin.h"

namespace base {
class Time;
}  // namespace base

namespace net {
class SchemefulSite;
}  // namespace net

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
  using BudgetResult = SharedStorageDatabase::BudgetResult;
  using TimeResult = SharedStorageDatabase::TimeResult;
  using MetadataResult = SharedStorageDatabase::MetadataResult;
  using EntriesResult = SharedStorageDatabase::EntriesResult;

  // A callback type to check if a given StorageKey matches a storage policy.
  // Can be passed empty/null where used, which means the StorageKey will always
  // match.
  using StorageKeyPolicyMatcherFunction =
      SharedStorageDatabase::StorageKeyPolicyMatcherFunction;

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

  // Called when the system is under memory pressure.
  void OnMemoryPressure(
      base::OnceCallback<void()> callback,
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Tallies database errors, watching for consecutive ones. If the threshold
  // `max_allowed_consecutive_operation_errors_` is exceeded, then the database
  // is deleted and recreated in an attempt to recover.
  void OnOperationResult(OperationResult result);

  // Retrieves the `value` for `context_origin` and `key`. `callback` is called
  // with a struct bundling a string `value` in its data field if one is found,
  // `std::nullopt` otherwise, and a OperationResult to indicate whether the
  // transaction was free of database errors.
  //
  // `key` must be of length at most
  // `SharedStorageDatabase::max_string_length_`, with the burden on the caller
  // to handle errors for strings that exceed this length.
  void Get(url::Origin context_origin,
           std::u16string key,
           base::OnceCallback<void(GetResult)> callback);

  // Sets an entry for `context_origin` and `key` to have `value`.
  // If `behavior` is `kIgnoreIfPresent` and an entry already exists for
  // `context_origin` and `key`, then the database table is not modified.
  // The parameter of `callback` reports whether or not any entry is added, the
  // request is ignored, or if there is an error.
  //
  // `key` and `value` must each be of length at most
  // `SharedStorageDatabase::max_string_length_`, with the burden on the caller
  // to handle errors for strings that exceed these lengths. Moreover, if the
  // bytes used retrieved by `BytesUsed(context_origin, callback)` plus any
  // additional bytes to be stored by this call would exceed
  // `SharedStorageDatabaseOptions::max_bytes_per_origin_`, `Set()` will fail
  // and the table will not be modified.
  void Set(url::Origin context_origin,
           std::u16string key,
           std::u16string value,
           base::OnceCallback<void(OperationResult)> callback,
           SetBehavior behavior);

  // Appends `value` to the end of the current `value` for `context_origin` and
  // `key`, if `key` exists. If `key` does not exist, creates an entry for `key`
  // with value `value`. The parameter of `callback` reports whether or not any
  // entry is added or modified or if there is an error.
  //
  // `key` and `value` must each be of length at most
  // `SharedStorageDatabase::max_string_length_`, with the burden on the caller
  // to handle errors for strings that exceed these lengths. Moreover, if the
  // length of the string obtained by concatening the current `script_value` (if
  // one exists) and `value` exceeds
  // `SharedStorageDatabase::max_string_length_`, or if the bytes used retrieved
  // by `ByresUsed(context_origin, callback)` plus any additional bytes to be
  // stored by this call would exceed
  // `SharedStorageDatabaseOptions::max_bytes_per_origin_`, `Append()` will fail
  // and the database table will not be modified.
  void Append(url::Origin context_origin,
              std::u16string key,
              std::u16string value,
              base::OnceCallback<void(OperationResult)> callback);

  // Deletes the entry for `context_origin` and `key`. The parameter of
  // `callback` reports whether the deletion is successful.
  //
  // `key` must be of length at most
  // `SharedStorageDatabase::max_string_length_`, with the burden on the caller
  // to handle errors for strings that exceed this length.
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
            mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
                pending_listener,
            base::OnceCallback<void(OperationResult)> callback);

  // From a list of all the key-value pairs for `context_origin` taken in
  // lexicographic order, send batches of key-value pairs to the Shared Storage
  // worklet's async iterator via a remote that consumes `pending_listener`.
  // Calls `callback` with an OperationResult to indicate whether the
  // transaction was successful.
  void Entries(url::Origin context_origin,
               mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
                   pending_listener,
               base::OnceCallback<void(OperationResult)> callback);

  // Clears all entries for `context_origin`. The parameter of `callback`
  // reports whether the operation is successful. Can be called either as part
  // of the Shared Storage API, or else by
  // `browsing_data::SharedStorageHelper::DeleteOrigin()` in order to clear
  // browsing data via the Settings UI.
  void Clear(url::Origin context_origin,
             base::OnceCallback<void(OperationResult)> callback);

  // The parameter of `callback` reports the number of bytes used by
  // `context_origin` in unexpired entries, 0 if the origin has no unexpired
  // entries, or -1 on operation failure.
  void BytesUsed(url::Origin context_origin,
                 base::OnceCallback<void(int)> callback);

  // Clears all StorageKeys that match `storage_key_matcher` run on the owning
  // StoragePartition's `SpecialStoragePolicy` and have `last_used_time` between
  // the times `begin` and `end`. If `perform_storage_cleanup` is true, vacuums
  // the database afterwards. The parameter of `callback` reports whether the
  // transaction was successful. Called by
  // `content::StoragePartitionImpl::DataDeletionHelper::ClearDataOnUIThread()`.
  //
  // Note that `storage_key_matcher` is accessed on a different sequence than
  // where it was created.
  void PurgeMatchingOrigins(StorageKeyPolicyMatcherFunction storage_key_matcher,
                            base::Time begin,
                            base::Time end,
                            base::OnceCallback<void(OperationResult)> callback,
                            bool perform_storage_cleanup = false);

  // Fetches a vector of `mojom::StorageUsageInfoPtr`, with one
  // `mojom::StorageUsageInfoPtr` for each origin currently using shared
  // storage in this profile. Called by
  // `browsing_data::SharedStorageHelper::StartFetching`.
  void FetchOrigins(
      base::OnceCallback<void(std::vector<mojom::StorageUsageInfoPtr>)>
          callback);

  // Makes a withdrawal of `bits_debit` stamped with the current time from the
  // privacy budget of `context_site`.
  void MakeBudgetWithdrawal(net::SchemefulSite context_site,
                            double bits_debit,
                            base::OnceCallback<void(OperationResult)> callback);

  // Determines the number of bits remaining in the privacy budget of
  // `context_site`, where only withdrawals within the most recent
  // `budget_interval_` are counted as still valid, and calls `callback` with
  // this information bundled with an `OperationResult` value to indicate
  // whether the database retrieval was successful.
  void GetRemainingBudget(net::SchemefulSite context_site,
                          base::OnceCallback<void(BudgetResult)> callback);

  // Calls `callback` with the most recent creation time (currently in the
  // schema as `last_used_time`) for `context_origin` and an `OperationResult`
  // to indicate whether or not there were errors.
  void GetCreationTime(url::Origin context_origin,
                       base::OnceCallback<void(TimeResult)> callback);

  // Calls `SharedStorageDatabase::Length()`,
  // `SharedStorageDatabase::GetRemainingBudget()`, and
  // `SharedStorageDatabase::GetCreationTime()`, then bundles this info along
  // with the accompanying `OperationResult`s into a struct to send to the
  // DevTools `StorageHandler` via `callback`. Because DevTools displays
  // shared storage data by origin, we continue to pass a `url::Origin` in as
  // parameter `context_origin` and compute the site on the fly to use as
  // parameter for `GetRemainingBudget()`.
  void GetMetadata(url::Origin context_origin,
                   base::OnceCallback<void(MetadataResult)> callback);

  // Calls `callback` with an origin's entries in a vector bundled with an
  // `OperationResult`. To only be used by DevTools.
  void GetEntriesForDevTools(url::Origin context_origin,
                             base::OnceCallback<void(EntriesResult)> callback);

  // Removes all budget withdrawals for `context_origin`'s site. Calls
  // `callback` to indicate whether the transaction succeeded. Intended as a
  // convenience for the DevTools UX. Because DevTools displays shared storage
  // data by origin, we continue to pass a `url::Origin` in as parameter
  // `context_origin` and compute the site on the fly.
  void ResetBudgetForDevTools(
      url::Origin context_origin,
      base::OnceCallback<void(OperationResult)> callback);

  void SetOnDBDestroyedCallbackForTesting(
      base::OnceCallback<void(bool)> callback);

  void OverrideCreationTimeForTesting(url::Origin context_origin,
                                      base::Time new_creation_time,
                                      base::OnceCallback<void(bool)> callback);

  void OverrideSpecialStoragePolicyForTesting(
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);

  void OverrideClockForTesting(base::Clock* clock, base::OnceClosure callback);

  void OverrideDatabaseForTesting(
      std::unique_ptr<AsyncSharedStorageDatabase> override_async_database);

  // Calls `callback` with the number of entries (including stale entries) in
  // the table `budget_mapping` for `context_site`, or with -1 in case of
  // database initialization failure or SQL error.
  void GetNumBudgetEntriesForTesting(net::SchemefulSite context_site,
                                     base::OnceCallback<void(int)> callback);

  // Calls `callback` with the total number of entries in the table for all
  // origins, or with -1 in case of database initialization failure or SQL
  // error.
  void GetTotalNumBudgetEntriesForTesting(
      base::OnceCallback<void(int)> callback);

 private:
  void DestroyAndRecreateDatabase();
  void OnDatabaseDestroyed(bool recreate_in_memory, bool success);

  // Returns a new callback that also includes a call to `OnOperationResult()`.
  base::OnceCallback<void(OperationResult)> GetOperationResultCallback(
      base::OnceCallback<void(OperationResult)> callback);

  // Clear all entries whose `last_used_time` (currently the last write access)
  // falls before `SharedStorageDatabase::clock_->Now() -
  // options_->staleness_threshold_`. Also purges, for all origins, all privacy
  // budget withdrawals that have `time_stamps` older than
  // `SharedStorageDatabase::clock_->Now() - options_->budget_interval_`. The
  // parameter of `callback` reports whether the transaction was successful.
  void PurgeStale();

  // Starts the `timer_` for the next call to `PurgeStale()`.
  void OnStalePurged(OperationResult result);

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
