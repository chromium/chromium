// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_ASYNC_SHARED_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_ASYNC_SHARED_STORAGE_DATABASE_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/shared_storage/public/mojom/shared_storage.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace base {
class Time;
class TimeDelta;
}  // namespace base

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

  // A callback type to check if a given origin matches a storage policy.
  // Can be passed empty/null where used, which means the origin will always
  // match.
  using OriginMatcherFunction = SharedStorageDatabase::OriginMatcherFunction;

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
  // `Length()`, `Keys()`, `Entries()`, `PurgeMatchingOrigins()`,
  // `PurgeStaleOrigins()`, and `FetchOrigins()` are all async versions of the
  // corresponding methods in `storage::SharedStorageDatabase`, with the
  // modification that `Set()` and `Append()` take a boolean callback to
  // indicate that a value was set or appended, rather than a long integer
  // callback with the row number for the next available row.
  //
  // It is OK to call these async methods even if the database has failed to
  // initialize, as there is an alternate code path to handle this case that
  // skips accessing `database_` (as it will be null) and hence performing the
  // intending operation, logs the occurrence of the missing database to UMA,
  // and runs the callback with a trivial instance of its expected result type).

  // Releases all non-essential memory associated with this database connection.
  // `callback` runs once the operation is finished.
  virtual void TrimMemory(base::OnceClosure callback) = 0;

  // Retrieves the `value` for `context_origin` and `key`. `callback` is called
  // with a struct bundling a string `value` in its data field if one is found,
  // `absl::nullopt` otherwise, and a OperationResult to indicate whether the
  // transaction was free of database errors.
  //
  // `key` must be of length at most
  // `SharedStorageDatabaseOptions::max_string_length`, with the burden on the
  // caller to handle errors for strings that exceed this length.
  virtual void Get(url::Origin context_origin,
                   std::u16string key,
                   base::OnceCallback<void(GetResult)> callback) = 0;

  // Sets an entry for `context_origin` and `key` to have `value`.
  // If `behavior` is `kIgnoreIfPresent` and an entry already exists for
  // `context_origin` and `key`, then the database table is not modified.
  // The parameter of `callback` reports whether or not any entry is added, the
  // request is ignored, or if there is an error.
  //
  // `key` and `value` must be each of length at most
  // `SharedStorageDatabaseOptions::max_string_length`, with the burden on the
  // caller to handle errors for strings that exceed this length. Moreover, if
  // the length retrieved by `Length(context_origin, callback)` equals
  // `SharedStorageDatabaseOptions::max_entries_per_origin_`, `Set()` will fail
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
  // `key` and `value` must be each of length at most
  // `SharedStorageDatabaseOptions::max_string_length`, with the burden on the
  // caller to handle errors for strings that exceed this length. Moreover, if
  // the length of the string obtained by concatening the current `script_value`
  // (if one exists) and `value` exceeds
  // `SharedStorageDatabaseOptions::max_string_length`, or if the length
  // retrieved by `Length(context_origin, callback)` equals
  // `SharedStorageDatabaseOptions::max_entries_per_origin_`, `Append()` will
  // fail and the database table will not be modified.
  virtual void Append(url::Origin context_origin,
                      std::u16string key,
                      std::u16string value,
                      base::OnceCallback<void(OperationResult)> callback) = 0;

  // Deletes the entry for `context_origin` and `key`. The parameter of
  // `callback` reports whether the deletion is successful.
  //
  // `key` must be of length at most
  // `SharedStorageDatabaseOptions::max_string_length`, with the burden on the
  // caller to handle errors for strings that exceed this length.
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
      mojo::PendingRemote<
          shared_storage_worklet::mojom::SharedStorageEntriesListener>
          pending_listener,
      base::OnceCallback<void(OperationResult)> callback) = 0;

  // From a list of all the key-value pairs for `context_origin` taken in
  // lexicographic order, send batches of key-value pairs to the Shared Storage
  // worklet's async iterator via a remote that consumes `pending_listener`.
  // Calls `callback` with an OperationResult to indicate whether the
  // transaction was successful.
  virtual void Entries(
      url::Origin context_origin,
      mojo::PendingRemote<
          shared_storage_worklet::mojom::SharedStorageEntriesListener>
          pending_listener,
      base::OnceCallback<void(OperationResult)> callback) = 0;

  // Clears all origins that match `origin_matcher` run on the owning
  // StoragePartition's `SpecialStoragePolicy` and have `last_used_time` between
  // the times `begin` and `end`. If `perform_storage_cleanup` is true, vacuums
  // the database afterwards. The parameter of `callback` reports whether the
  // transaction was successful.
  //
  // Note that `origin_matcher` is accessed on a different sequence than where
  // it was created.
  virtual void PurgeMatchingOrigins(
      OriginMatcherFunction origin_matcher,
      base::Time begin,
      base::Time end,
      base::OnceCallback<void(OperationResult)> callback,
      bool perform_storage_cleanup = false) = 0;

  // Clear all entries for all origins whose `last_read_time` falls before
  // `base::Time::Now() - window_to_be_deemed_active`.
  virtual void PurgeStaleOrigins(
      base::TimeDelta window_to_be_deemed_active,
      base::OnceCallback<void(OperationResult)> callback) = 0;

  // Fetches a vector of `mojom::StorageUsageInfoPtr`, with one
  // `mojom::StorageUsageInfoPtr` for each origin currently using shared storage
  // in this profile.
  virtual void FetchOrigins(
      base::OnceCallback<void(std::vector<mojom::StorageUsageInfoPtr>)>
          callback) = 0;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_ASYNC_SHARED_STORAGE_DATABASE_H_
