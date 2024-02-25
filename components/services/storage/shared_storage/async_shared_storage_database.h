// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_ASYNC_SHARED_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_ASYNC_SHARED_STORAGE_DATABASE_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"

namespace base {
class Time;
}  // namespace base

namespace net {
class SchemefulSite;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace storage {
struct SharedStorageDatabaseOptions;
class SpecialStoragePolicy;

// A wrapper around SharedStorageDatabase which makes the operations
// asynchronous.
class AsyncSharedStorageDatabase {
 public:
  using InitStatus = SharedStorageDatabase::InitStatus;
  using SetBehavior = SharedStorageDatabase::SetBehavior;
  using OperationResult = SharedStorageDatabase::OperationResult;
  using GetResult = SharedStorageDatabase::GetResult;
  using BudgetResult = SharedStorageDatabase::BudgetResult;
  using TimeResult = SharedStorageDatabase::TimeResult;
  using MetadataResult = SharedStorageDatabase::MetadataResult;
  using EntriesResult = SharedStorageDatabase::EntriesResult;

  // A callback type to check if a given origin matches a storage policy.
  // Can be passed empty/null where used, which means the origin will always
  // match.
  using StorageKeyPolicyMatcherFunction =
      SharedStorageDatabase::StorageKeyPolicyMatcherFunction;

  virtual ~AsyncSharedStorageDatabase() = default;

  // Destroys the database.
  //
  // If filebacked, deletes the persistent database within the filesystem
  // directory.
  //
  // It is OK to call `Destroy()` regardless of whether database initialization
  // was successful.
  virtual void Destroy(base::OnceCallback<void(bool)> callback) = 0;

  // `TrimMemory()`, `Get()`, `Set()`, `Append()`, `Delete()`, `Clear()`,
  // `Length()`, `Keys()`, `Entries()`, `BytesUsed()`, `PurgeMatchingOrigins()`,
  // `PurgeStale()`, `FetchOrigins()`, `MakeBudgetWithdrawal()`,
  // `GetRemainingBudget()`, `GetCreationTime()`, `GetMetadata()`,
  // `GetEntriesForDevTools()`, and `ResetBudgetForDevTools() are all async
  // versions of the corresponding methods in `storage::SharedStorageDatabase`,
  // with the modification that `Set()` and `Append()` take a boolean callback
  // to indicate that a value was set or appended, rather than a long integer
  // callback with the row number for the next available row.
  //
  // It is OK to call these async methods even if the database has failed to
  // initialize, as there is an alternate code path to handle this case that
  // skips accessing `database_` (as it will be null) and hence performing the
  // intending operation, logs the occurrence of the missing database to UMA,
  // and runs the callback with a trivial instance of its expected result type.

  // Releases all non-essential memory associated with this database connection.
  // `callback` runs once the operation is finished.
  virtual void TrimMemory(base::OnceClosure callback) = 0;

  // Retrieves the `value` for `context_origin` and `key`. `callback` is called
  // with a struct bundling a string `value` in its data field if one is found,
  // `std::nullopt` otherwise, and a OperationResult to indicate whether the
  // transaction was free of database errors.
  //
  // `key` must be of length at most
  // `SharedStorageDatabase::max_string_length_`, with the burden on the caller
  // to handle errors for strings that exceed this length.
  virtual void Get(url::Origin context_origin,
                   std::u16string key,
                   base::OnceCallback<void(GetResult)> callback) = 0;

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
  virtual void Set(url::Origin context_origin,
                   std::u16string key,
                   std::u16string value,
                   base::OnceCallback<void(OperationResult)> callback,
                   SetBehavior behavior = SetBehavior::kDefault) = 0;

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
  virtual void Append(url::Origin context_origin,
                      std::u16string key,
                      std::u16string value,
                      base::OnceCallback<void(OperationResult)> callback) = 0;

  // Deletes the entry for `context_origin` and `key`. The parameter of
  // `callback` reports whether the deletion is successful.
  //
  // `key` must be of length at most
  // `SharedStorageDatabase::max_string_length_`, with the burden on the caller
  // to handle errors for strings that exceed this length.
  virtual void Delete(url::Origin context_origin,
                      std::u16string key,
                      base::OnceCallback<void(OperationResult)> callback) = 0;

  // Clears all entries for `context_origin`. The parameter of `callback`
  // reports whether the operation is successful.
  virtual void Clear(url::Origin context_origin,
                     base::OnceCallback<void(OperationResult)> callback) = 0;

  // The parameter of `callback` reports the number of entries for
  // `context_origin`, 0 if there are none, or -1 on operation failure.
  virtual void Length(url::Origin context_origin,
                      base::OnceCallback<void(int)> callback) = 0;

  // From a list of all the keys for `context_origin` taken in lexicographic
  // order, send batches of keys to the Shared Storage worklet's async iterator
  // via a remote that consumes `pending_listener`. Calls `callback` with an
  // OperationResult to indicate whether the transaction was successful.
  virtual void Keys(
      url::Origin context_origin,
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          pending_listener,
      base::OnceCallback<void(OperationResult)> callback) = 0;

  // From a list of all the key-value pairs for `context_origin` taken in
  // lexicographic order, send batches of key-value pairs to the Shared Storage
  // worklet's async iterator via a remote that consumes `pending_listener`.
  // Calls `callback` with an OperationResult to indicate whether the
  // transaction was successful.
  virtual void Entries(
      url::Origin context_origin,
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          pending_listener,
      base::OnceCallback<void(OperationResult)> callback) = 0;

  // The parameter of `callback` reports the number of bytes used by
  // `context_origin` in unexpired entries, 0 if the origin has no unexpired
  // entries, or -1 on operation failure.
  virtual void BytesUsed(url::Origin context_origin,
                         base::OnceCallback<void(int)> callback) = 0;

  // Clears all origins that match `storage_key_matcher` run on the owning
  // StoragePartition's `SpecialStoragePolicy` and have any key with
  // `last_used_time` between the times `begin` and `end`. If
  // `perform_storage_cleanup` is true, vacuums the database afterwards. The
  // parameter of `callback` reports whether the transaction was successful.
  //
  // Note that `storage_key_matcher` is accessed on a different sequence than
  // where it was created.
  virtual void PurgeMatchingOrigins(
      StorageKeyPolicyMatcherFunction storage_key_matcher,
      base::Time begin,
      base::Time end,
      base::OnceCallback<void(OperationResult)> callback,
      bool perform_storage_cleanup = false) = 0;

  // Clear all entries whose `last_used_time` (currently the last write access)
  // falls before `SharedStorageDatabase::clock_->Now() - staleness_threshold_`.
  // Also purges, for all origins, all privacy budget withdrawals that have
  // `time_stamps` older than `SharedStorageDatabase::clock_->Now() -
  // budget_interval_`. The parameter of `callback` reports whether the
  // transaction was successful.
  virtual void PurgeStale(
      base::OnceCallback<void(OperationResult)> callback) = 0;

  // Fetches a vector of `mojom::StorageUsageInfoPtr`, with one
  // `mojom::StorageUsageInfoPtr` for each origin currently using shared
  // storage in this profile.
  virtual void FetchOrigins(
      base::OnceCallback<void(std::vector<mojom::StorageUsageInfoPtr>)>
          callback) = 0;

  // Makes a withdrawal of `bits_debit` stamped with the current time from the
  // privacy budget of `context_site`.
  virtual void MakeBudgetWithdrawal(
      net::SchemefulSite context_site,
      double bits_debit,
      base::OnceCallback<void(OperationResult)> callback) = 0;

  // Determines the number of bits remaining in the privacy budget of
  // `context_site`, where only withdrawals within the most recent
  // `budget_interval_` are counted as still valid, and calls `callback` with
  // this information bundled with an `OperationResult` value to indicate
  // whether the database retrieval was successful.
  virtual void GetRemainingBudget(
      net::SchemefulSite context_site,
      base::OnceCallback<void(BudgetResult)> callback) = 0;

  // Calls `callback` with the most recent creation time (currently in the
  // schema as `last_used_time`) for `context_origin` and an `OperationResult`
  // to indicatewhether or not there were errors.
  virtual void GetCreationTime(
      url::Origin context_origin,
      base::OnceCallback<void(TimeResult)> callback) = 0;

  // Calls `SharedStorageDatabase::Length()`,
  // `SharedStorageDatabase::GetRemainingBudget()`, and
  // `SharedStorageDatabase::GetCreationTime()`, then bundles this info along
  // with the accompanying `OperationResult`s into a struct to send to the
  // DevTools `StorageHandler` via `callback`. Because DevTools displays
  // shared storage data by origin, we continue to pass a `url::Origin` in as
  // parameter `context_origin` and compute the site on the fly to use as
  // parameter for `GetRemainingBudget()`.
  virtual void GetMetadata(
      url::Origin context_origin,
      base::OnceCallback<void(MetadataResult)> callback) = 0;

  // Calls `callback` with an origin's entries in a vector bundled with an
  // `OperationResult`. To only be used by DevTools.
  virtual void GetEntriesForDevTools(
      url::Origin context_origin,
      base::OnceCallback<void(EntriesResult)> callback) = 0;

  // Removes all budget withdrawals for `context_origin`'s site. Calls
  // `callback` to indicate whether the transaction succeeded. Intended as a
  // convenience for the DevTools UX. Because DevTools displays shared storage
  // data by origin, we continue to pass a `url::Origin` in as parameter
  // `context_origin` and compute the site on the fly.
  virtual void ResetBudgetForDevTools(
      url::Origin context_origin,
      base::OnceCallback<void(OperationResult)> callback) = 0;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_ASYNC_SHARED_STORAGE_DATABASE_H_
