// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_DATABASE_H_

#include <inttypes.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/time/clock.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom-forward.h"
#include "components/services/storage/shared_storage/public/mojom/shared_storage.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
class Time;
class TimeDelta;
}  // namespace base

namespace sql {
class Statement;
}

namespace url {
class Origin;
}  // namespace url

namespace storage {
struct SharedStorageDatabaseOptions;
class SpecialStoragePolicy;

// Multiplier for determining the padded total size in bytes that an origin
// is using.
extern const int kSharedStorageEntryTotalBytesMultiplier;

// Wraps its own `sql::Database` instance on behalf of the Shared Storage
// backend implementation. This object is not sequence-safe and must be
// instantiated on a sequence which allows use of blocking file operations.
class SharedStorageDatabase {
 public:
  // A callback type to check if a given origin matches a storage policy.
  // Can be passed empty/null where used, which means the origin will always
  // match.
  using OriginMatcherFunction =
      base::RepeatingCallback<bool(const url::Origin&, SpecialStoragePolicy*)>;

  enum class InitStatus {
    kUnattempted =
        0,  // Status if `LazyInit()` has not yet been called or if `LazyInit()`
            // has early returned due to `DBCreationPolicy::kIgnoreIfAbsent`.
    kSuccess = 1,  // Status if `LazyInit()` was successful.
    kError = 2,    // Status if `LazyInit()` failed and a more specific error
                   // wasn't diagnosed.
    kTooNew = 3,   // Status if `LazyInit()` failed due to a compatible version
                   // number being too high.
    kTooOld = 4,  // Status if `LazyInit()` failed due to a version number being
                  // too low.
  };

  enum class DBFileStatus {
    kNotChecked = 0,  // Status if DB is file-backed and there hasn't been an
                      // attempt to open the SQL database for the given FilePath
                      // to see if it exists and contains data.
    kNoPreexistingFile =
        1,  // Status if the DB is in-memory or if the DB is file-backed but the
            // attempt to open it was unsuccessful or any pre-existing file
            // contained no data.
    kPreexistingFile =
        2,  // Status if there was a pre-existing file containing at least one
            // table that we were able to successfully open.
  };

  enum class SetBehavior {
    kDefault = 0,  // Sets entry regardless of whether one previously exists.
    kIgnoreIfPresent = 1,  // Does not set an entry if one previously exists.
  };

  enum class OperationResult {
    kSuccess = 0,      // Result if a non-setting operation is successful.
    kSet = 1,          // Result if value is set.
    kIgnored = 2,      // Result if value was present and ignored; no error.
    kSqlError = 3,     // Result if there is a SQL database error.
    kInitFailure = 4,  // Result if database initialization failed and a
                       // database is required.
    kNoCapacity = 5,   // Result if there was insufficient capacity for the
    // requesting origin.
    kInvalidAppend = 6,  // Result if the length of the value after appending
    // would exceed the maximum allowed length.
  };

  // Bundles a retrieved string from the database along with a field indicating
  // whether the transaction was free of SQL errors.
  struct GetResult {
    absl::optional<std::u16string> data;
    OperationResult result = OperationResult::kSqlError;
    GetResult();
    GetResult(const GetResult&);
    GetResult(GetResult&&);
    ~GetResult();
    GetResult& operator=(const GetResult&);
    GetResult& operator=(GetResult&&);
  };

  // When `db_path` is empty, the database will be opened in memory only.
  SharedStorageDatabase(
      base::FilePath db_path,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<SharedStorageDatabaseOptions> options);

  SharedStorageDatabase(const SharedStorageDatabase&) = delete;
  SharedStorageDatabase(const SharedStorageDatabase&&) = delete;

  ~SharedStorageDatabase();

  SharedStorageDatabase& operator=(const SharedStorageDatabase&) = delete;
  SharedStorageDatabase& operator=(const SharedStorageDatabase&&) = delete;

  // Deletes the database and returns whether the operation was successful.
  //
  // It is OK to call `Destroy()` regardless of whether `Init()` was successful.
  [[nodiscard]] bool Destroy();

  // Returns a pointer to the database containing the actual data.
  [[nodiscard]] sql::Database* db() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return &db_;
  }

  // Releases all non-essential memory associated with this database connection.
  void TrimMemory();

  // Retrieves the `entry` for `context_origin` and `key`. Returns a
  // struct containing a `value` string if one is found, `absl::nullopt`
  // otherwise, with a bool `success` indicating whether the transaction was
  // free of errors.
  //
  // Note that `key` is assumed to be of length at most
  // `max_string_length_`, with the burden on the caller to handle errors for
  // strings that exceed this length.
  [[nodiscard]] GetResult Get(url::Origin context_origin, std::u16string key);

  // Sets an entry for `context_origin` and `key` to have `value`.
  // If `behavior` is `kIgnoreIfPresent` and an entry already exists for
  // `context_origin` and `key`, then the table is not modified.
  // Returns an enum indicating whether or not a new entry is added, the request
  // is ignored, or if there is an error.
  //
  // Note that `key` and `value` assumed to be each of length at
  // most `max_string_length_`, with the burden on the caller to handle errors
  // for strings that exceed this length. Moreover, if `Length(context_origin)`
  // equals `max_entries_per_origin_`, `Set()` will return a value of
  // `OperationResult::kNoCapacity` and the table will not be modified.
  [[nodiscard]] OperationResult Set(
      url::Origin context_origin,
      std::u16string key,
      std::u16string value,
      SetBehavior behavior = SetBehavior::kDefault);

  // Appends `tail_value` to the end of the current `value`
  // for `context_origin` and `key`, if `key` exists. If
  // `key` does not exist, creates an entry for `key` with value
  // `tail_value`. Returns an enum indicating whether or not an entry is
  // added/modified or if there is an error.
  //
  // Note that `key` and `value` are assumed to be each of length
  // at most `max_string_length_`, with the burden on the caller to handle
  // errors for strings that exceed this length. Moreover, if the length of the
  // string obtained by concatening the current `value` (if one exists)
  // and `tail_value` exceeds `max_string_length_`, or if
  // `Length(context_origin)` equals `max_entries_per_origin_`, `Append()` will
  // return a value of `OperationResult::kNoCapacity` and the table will not be
  // modified.
  [[nodiscard]] OperationResult Append(url::Origin context_origin,
                                       std::u16string key,
                                       std::u16string tail_value);

  // Deletes the entry for `context_origin` and `key`. Returns
  // whether the deletion is successful.
  //
  // Note that `key` is assumed to be of length at most
  // `max_string_length_`, with the burden on the caller to handle errors for
  // strings that exceed this length.
  [[nodiscard]] OperationResult Delete(url::Origin context_origin,
                                       std::u16string key);

  // Clears all entries for `context_origin`. Returns whether the operation is
  // successful.
  [[nodiscard]] OperationResult Clear(url::Origin context_origin);

  // Returns the number of entries for `context_origin` in the database, or -1
  // on error. Note that this call will update the origin's `last_used_time`.
  // TODO(crbug.com/1277662): Consider renaming to something more descriptive.
  [[nodiscard]] int64_t Length(url::Origin context_origin);

  // From a list of all the keys for `context_origin` taken in lexicographic
  // order, send batches of keys to the Shared Storage worklet's async iterator
  // via a remote that consumes `pending_listener`. Returns whether the
  // operation was successful.
  [[nodiscard]] OperationResult Keys(
      const url::Origin& context_origin,
      mojo::PendingRemote<
          shared_storage_worklet::mojom::SharedStorageEntriesListener>
          pending_listener);

  // From a list of all the key-value pairs for `context_origin` taken in
  // lexicographic order, send batches of key-value pairs to the Shared Storage
  // worklet's async iterator via a remote that consumes `pending_listener`.
  // Returns whether the operation was successful.
  [[nodiscard]] OperationResult Entries(
      const url::Origin& context_origin,
      mojo::PendingRemote<
          shared_storage_worklet::mojom::SharedStorageEntriesListener>
          pending_listener);

  // Clears all origins that match `origin_matcher` run on the owning
  // StoragePartition's `SpecialStoragePolicy` and have `last_used_time` between
  // the times `begin` and `end`. If `perform_storage_cleanup` is true, vacuums
  // the database afterwards. Returns whether the transaction was successful.
  [[nodiscard]] OperationResult PurgeMatchingOrigins(
      OriginMatcherFunction origin_matcher,
      base::Time begin,
      base::Time end,
      bool perform_storage_cleanup = false);

  // Clear all entries for all origins whose `last_read_time` falls before
  // `base::Time::Now() - window_to_be_deemed_active`. Returns whether the
  // transaction was successful.
  [[nodiscard]] OperationResult PurgeStaleOrigins(
      base::TimeDelta window_to_be_deemed_active);

  // Fetches a vector of `mojom::StorageUsageInfoPtr`, with one
  // `mojom::StorageUsageInfoPtr` for each origin currently using shared storage
  // in this profile.
  [[nodiscard]] std::vector<mojom::StorageUsageInfoPtr> FetchOrigins();

  // Returns whether the SQLite database is open.
  [[nodiscard]] bool IsOpenForTesting() const;

  // Returns the `db_status_` for tests.
  [[nodiscard]] InitStatus DBStatusForTesting() const;

  // Changes `last_used_time` to `new_last_used_time` for `context_origin`.
  [[nodiscard]] bool OverrideLastUsedTimeForTesting(
      url::Origin context_origin,
      base::Time new_last_used_time);

  // Overrides the clock used to check the time.
  void OverrideClockForTesting(base::Clock* clock);

  // Overrides the `SpecialStoragePolicy` for tests.
  void OverrideSpecialStoragePolicyForTesting(
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);

  // Populates the database in order to test integration with
  // `content::StoragePartitionImpl` while keeping in this file the parts of
  // those tests that depend on implementation details of
  // `SharedStorageDatabase`.
  //
  // Sets two example key-value pairs for `origin1`, one example pair for
  // `origin2`, and two example pairs for `origin3`, while also overriding the
  // `last_used_time` for `origin2` so that it is 1 day earlier and the
  // `last_used_time` for `origin3` so that it is 60 days earlier.
  [[nodiscard]] bool PopulateDatabaseForTesting(url::Origin origin1,
                                                url::Origin origin2,
                                                url::Origin origin3);

 private:
  // Policy to tell `LazyInit()` whether or not to create a new database if a
  // pre-existing on-disk database is not found.
  enum class DBCreationPolicy {
    kIgnoreIfAbsent = 0,
    kCreateIfAbsent = 1,
  };

  // Called at the start of each public operation, and initializes the database
  // if it isn't already initialized (unless there is no pre-existing on-disk
  // database to initialize and `policy` is
  // `DBCreationPolicy::kIgnoreIfAbsent`).
  [[nodiscard]] InitStatus LazyInit(DBCreationPolicy policy)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Determines whether or not an uninitialized DB already exists on disk.
  [[nodiscard]] bool DBExists() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // If `db_path_` is empty, opens a temporary database in memory; otherwise
  // opens a persistent database with the absolute path `db_path`, creating the
  // file if it does not yet exist. Returns whether opening was successful.
  [[nodiscard]] bool OpenDatabase() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Callback for database errors. Schedules a call to Destroy() if the
  // error is catastrophic.
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Helper function to implement internals of `Init()`.  This allows
  // Init() to retry in case of failure, since some failures run
  // recovery code.
  [[nodiscard]] InitStatus InitImpl() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Vacuums the database. This will cause sqlite to defragment and collect
  // unused space in the file. It can be VERY SLOW. Returns whether the
  // operation was successful.
  [[nodiscard]] bool Vacuum() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Clears all entries for `context_origin`. Returns whether deletion is
  // successful. Not named `Clear()` to distinguish it from the public method
  // called via `SequenceBound::AsyncCall()`.
  [[nodiscard]] bool Purge(const std::string& context_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns the number of entries for `context_origin`, i.e. the `length`.
  // Not named `Length()` to distinguish it from the public method called via
  // `SequenceBound::AsyncCall()`.
  [[nodiscard]] int64_t NumEntries(const std::string& context_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns whether an entry exists for `context_origin` and `key`.
  [[nodiscard]] bool HasEntryFor(const std::string& context_origin,
                                 const std::u16string& key)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Sets `last_used_time` to `new_last_used_time` for `context_origin`.
  [[nodiscard]] bool SetLastUsedTime(const std::string& context_origin,
                                     base::Time new_last_used_time)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Updates `last_used_time` to `base::Time::Now()` for `context_origin`.
  [[nodiscard]] bool UpdateLastUsedTime(const std::string& context_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Updates `length` by `delta` for `context_origin`. If `should_update_time`
  // is true, also updates `last_used_time` to `base::Time::Now()`.
  [[nodiscard]] bool UpdateLength(const std::string& context_origin,
                                  int64_t delta,
                                  bool should_update_time = true)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Inserts a triple for `(context_origin,key,value)` into
  // `values_mapping`.
  [[nodiscard]] bool InsertIntoValuesMapping(const std::string& context_origin,
                                             const std::u16string& key,
                                             const std::u16string& value)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deletes the row for `context_origin` from `per_origin_mapping`.
  [[nodiscard]] bool DeleteFromPerOriginMapping(
      const std::string& context_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Inserts the triple for `(context_origin, last_used_time, length)` into
  // `per_origin_mapping`.
  [[nodiscard]] bool InsertIntoPerOriginMapping(
      const std::string& context_origin,
      base::Time last_used_time,
      uint64_t length) VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns whether the `length` for `context_origin` is less than
  // `max_entries_per_origin_`.
  [[nodiscard]] bool HasCapacity(const std::string& context_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // The database containing the actual data.
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Contains the version information.
  sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Initialization status of `db_`.
  GUARDED_BY_CONTEXT(sequence_checker_)
  InitStatus db_status_ = InitStatus::kUnattempted;

  // Only set to true if `DBExists()
  DBFileStatus db_file_status_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The path to the database, if file-backed.
  base::FilePath db_path_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The owning partition's storage policy.
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The maximum allowed number of entries per origin.
  const int64_t max_entries_per_origin_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The maximum size of a string input from any origin's script. Applies
  // separately to both script keys and script values.
  size_t max_string_length_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Maxmium number of times that SQL database attempts to initialize.
  size_t max_init_tries_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Maximum number of keys or key-value pairs returned per batch by the
  // async `Keys()` and `Entries()` iterators, respectively.
  size_t max_iterator_batch_size_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Clock used to determine current time. Can be overridden in tests.
  raw_ptr<base::Clock> clock_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_DATABASE_H_
