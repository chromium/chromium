// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_ASYNC_SHARED_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_ASYNC_SHARED_STORAGE_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"

namespace base {
class FilePath;
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

  // Creates an `AsyncSharedStorageDatabase` instance. If `db_path` is empty,
  // creates a temporary, in-memory database; otherwise creates a persistent
  // database within a filesystem directory given by `db_path`, which must be an
  // absolute path. If file-backed, the database may or may not already exist at
  // `db_path`, and if it doesn't, it will be created.
  //
  // The instance will be bound to and perform all operations on
  // `blocking_task_runner`, which must support blocking operations.
  static std::unique_ptr<AsyncSharedStorageDatabase> Create(
      base::FilePath db_path,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<SharedStorageDatabaseOptions> options);

  AsyncSharedStorageDatabase(const AsyncSharedStorageDatabase&) = delete;
  AsyncSharedStorageDatabase& operator=(const AsyncSharedStorageDatabase&) =
      delete;
  AsyncSharedStorageDatabase(const AsyncSharedStorageDatabase&&) = delete;
  AsyncSharedStorageDatabase& operator=(const AsyncSharedStorageDatabase&&) =
      delete;

  ~AsyncSharedStorageDatabase();

  base::SequenceBound<SharedStorageDatabase>&
  GetSequenceBoundDatabaseForTesting() {
    return database_;
  }

  // Destroys the database.
  //
  // If filebacked, deletes the persistent database within the filesystem
  // directory.
  //
  // It is OK to call `Destroy()` regardless of whether database initialization
  // was successful.
  void Destroy(base::OnceCallback<void(bool)> callback);

  // `TrimMemory()`, `Get()`, `Set()`, `Append()`, `Delete()`, `Clear()`,
  // `Length()`, `Key()`, `PurgeMatchingOrigins()`, `PurgeStaleOrigins()`, and
  // `FetchOrigins()` are all async versions of the corresponding methods in
  // `storage::SharedStorageDatabase`, with the modification that `Set()` and
  // `Append()` take a boolean callback to indicate that a value was set or
  // appended, rather than a long integer callback with the row number for the
  // next available row.
  //
  // It is OK to call these async methods even if the database has failed to
  // initialize, as there is an alternate code path to handle this case that
  // skips accessing `database_` (as it will be null) and hence performing the
  // intending operation, logs the occurrence of the missing database to UMA,
  // and runs the callback with a trivial instance of its expected result type).

  // Releases all non-essential memory associated with this database connection.
  // `callback` runs once the operation is finished.
  void TrimMemory(base::OnceClosure callback);

  // Retrieves the `value` for `context_origin` and `key`. `callback` is called
  // with a struct bundling a string `value` in its data field if one is found,
  // `absl::nullopt` otherwise, and a OperationResult to indicate whether the
  // transaction was free of database errors.
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
  // The parameter of `callback` reports whether or not any entry is added, the
  // request is ignored, or if there is an error.
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

  // Clears all entries for `context_origin`. The parameter of `callback`
  // reports whether the operation is successful.
  void Clear(url::Origin context_origin,
             base::OnceCallback<void(OperationResult)> callback);

  // The parameter of `callback` reports the number of entries for
  // `context_origin`, 0 if there are none, or -1 on operation failure.
  void Length(url::Origin context_origin,
              base::OnceCallback<void(int)> callback);

  // If a list of all the keys for `context_origin` are taken in lexicographic
  // order, retrieves the `key` at `index` of the list and calls `callback` with
  // a struct bundling it as a parameter (along with a OperationResult to
  // indicate whether the transaction was free of database errors); otherwise
  // calls `callback` with `absl::nullopt` in the data field of the struct.
  // `index` must be non-negative.
  //
  // TODO(crbug.com/1247861): Replace with an async iterator.
  void Key(url::Origin context_origin,
           int index,
           base::OnceCallback<void(GetResult)> callback);

  // Clears all origins that match `origin_matcher` run on the owning
  // StoragePartition's `SpecialStoragePolicy` and have `last_used_time` between
  // the times `begin` and `end`. If `perform_storage_cleanup` is true, vacuums
  // the database afterwards. The parameter of `callback` reports whether the
  // transaction was successful.
  //
  // Note that `origin_matcher` is accessed on a different sequence than where
  // it was created.
  void PurgeMatchingOrigins(OriginMatcherFunction origin_matcher,
                            base::Time begin,
                            base::Time end,
                            base::OnceCallback<void(OperationResult)> callback,
                            bool perform_storage_cleanup = false);

  // Clear all entries for all origins whose `last_read_time` falls before
  // `base::Time::Now() - window_to_be_deemed_active`.
  void PurgeStaleOrigins(base::TimeDelta window_to_be_deemed_active,
                         base::OnceCallback<void(OperationResult)> callback);

  // Fetches a vector of `mojom::StorageUsageInfoPtr`, with one
  // `mojom::StorageUsageInfoPtr` for each origin currently using shared storage
  // in this profile.
  void FetchOrigins(
      base::OnceCallback<void(std::vector<mojom::StorageUsageInfoPtr>)>
          callback);

  // Asynchronously determines whether the database is open. Useful for testing.
  void IsOpenForTesting(base::OnceCallback<void(bool)> callback);

  // Asynchronously determines the database `InitStatus`. Useful for testing.
  void DBStatusForTesting(base::OnceCallback<void(InitStatus)> callback);

  // Changes `last_used_time` to `override_last_used_time` for `context_origin`.
  void OverrideLastUsedTimeForTesting(url::Origin context_origin,
                                      base::Time override_last_used_time,
                                      base::OnceCallback<void(bool)> callback);

  // Overrides the `SpecialStoragePolicy` for tests.
  void OverrideSpecialStoragePolicyForTesting(
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      base::OnceCallback<void(bool)> callback);

 private:
  // Instances should be obtained from the `Create()` factory method.
  AsyncSharedStorageDatabase(
      base::FilePath db_path,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<SharedStorageDatabaseOptions> options);

  base::SequenceBound<SharedStorageDatabase> database_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_ASYNC_SHARED_STORAGE_DATABASE_H_
