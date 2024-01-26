// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TESTING_LEGACY_SESSION_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TESTING_LEGACY_SESSION_STORAGE_DATABASE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace blink {
class StorageKey;
}

namespace leveldb {
class DB;
struct ReadOptions;
class WriteBatch;
}  // namespace leveldb

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}  // namespace base

namespace storage {

using LegacyDomStorageValuesMap =
    std::map<std::u16string, std::optional<std::u16string>>;

// A legacy implementation of Session Storage used only in tests to provide
// coverage of session storage migration code.
//
// This class is not thread safe. Read-only methods (ReadAreaValues,
// ReadNamespacesAndStorageKeys, and OnMemoryDump) may be called on any thread.
// Methods that modify the database must be called on the same thread.
class TestingLegacySessionStorageDatabase
    : public base::RefCountedDeleteOnSequence<
          TestingLegacySessionStorageDatabase> {
 public:
  // `file_path` is the path to the directory where the database will be
  // created. `commit_task_runner` is the runner on which methods which modify
  // the database must be run and where this object will be deleted.
  TestingLegacySessionStorageDatabase(
      const base::FilePath& file_path,
      scoped_refptr<base::SequencedTaskRunner> commit_task_runner);
  TestingLegacySessionStorageDatabase(
      const TestingLegacySessionStorageDatabase&) = delete;
  TestingLegacySessionStorageDatabase& operator=(
      const TestingLegacySessionStorageDatabase&) = delete;

  // Reads the (key, value) pairs for `namespace_id` and `storage_key`. `result`
  // is assumed to be empty and any duplicate keys will be overwritten. If the
  // database exists on disk then it will be opened. If it does not exist then
  // it will not be created and `result` will be unmodified.
  void ReadAreaValues(
      const std::string& namespace_id,
      const std::vector<std::string>& original_permanent_namespace_ids,
      const blink::StorageKey& storage_key,
      LegacyDomStorageValuesMap* result);

  // Updates the data for `namespace_id` and `storage_key`. Will remove all keys
  // before updating the database if `clear_all_first` is set. Then all entries
  // in `changes` will be examined - keys mapped to a nullopt value will be
  // removed and all others will be inserted/updated as appropriate. It is
  // allowed to write data into a shallow copy created by CloneNamespace, and in
  // that case the copy will be made deep before writing the values.
  bool CommitAreaChanges(const std::string& namespace_id,
                         const blink::StorageKey& storage_key,
                         bool clear_all_first,
                         const LegacyDomStorageValuesMap& changes);

  // Creates shallow copies of the areas for `namespace_id` and associates them
  // with `new_namespace_id`.
  bool CloneNamespace(const std::string& namespace_id,
                      const std::string& new_namespace_id);

  // Deletes the data for `namespace_id` and `storage_key`.
  bool DeleteArea(const std::string& namespace_id,
                  const blink::StorageKey& storage_key);

  // Deletes the data for `namespace_id`.
  bool DeleteNamespace(const std::string& namespace_id);

  // Reads the namespace IDs and storage_keys present in the database.
  bool ReadNamespacesAndStorageKeys(
      std::map<std::string, std::vector<blink::StorageKey>>*
          namespaces_and_storage_keys);

  // Adds memory statistics to `pmd` for chrome://tracing.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd);

  // Used in testing to set an easier to handle in-memory database. Should
  // happen before any database operations.
  void SetDatabaseForTesting(std::unique_ptr<leveldb::DB> db);

  leveldb::DB* db() const { return db_.get(); }

 private:
  class DBOperation;
  friend class TestingLegacySessionStorageDatabase::DBOperation;
  friend class base::RefCountedDeleteOnSequence<
      TestingLegacySessionStorageDatabase>;
  friend class base::DeleteHelper<TestingLegacySessionStorageDatabase>;
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaParamTest, ShallowCopyWithBacking);

  ~TestingLegacySessionStorageDatabase();

  // Opens the database at file_path_ if it exists already and creates it if
  // `create_if_needed` is true. Returns true if the database was opened, false
  // if the opening failed or was not necessary (the database doesn't exist and
  // `create_if_needed` is false). The possible failures are:
  // - leveldb cannot open the database.
  // - The database is in an inconsistent or errored state.
  bool LazyOpen(bool create_if_needed);

  // Tries to open the database at file_path_, assigns `db` to point to the
  // opened leveldb::DB instance.
  leveldb::Status TryToOpen(std::unique_ptr<leveldb::DB>* db);

  // Returns true if the database is already open, false otherwise.
  bool IsOpen() const;

  // Helpers for checking caller errors, invariants and database errors. All
  // these return `ok`, for chaining.
  bool CallerErrorCheck(bool ok) const;
  bool ConsistencyCheck(bool ok);
  bool DatabaseErrorCheck(bool ok);

  // Helper functions. All return true if the operation succeeded, and false if
  // it failed (a database error or a consistency error). If the return type is
  // void, the operation cannot fail. If they return false, ConsistencyCheck or
  // DatabaseErrorCheck have already been called.

  // Creates a namespace for `namespace_id` and updates the next namespace id if
  // needed. If `ok_if_exists` is false, checks that the namespace didn't exist
  // before.
  bool CreateNamespace(const std::string& namespace_id,
                       bool ok_if_exists,
                       leveldb::WriteBatch* batch);

  // Reads the areas associated with `namespace_id` and puts the
  // (serialized_origin, map_id) pairs into `areas`.
  bool GetAreasInNamespace(const std::string& namespace_id,
                           std::map<std::string, std::string>* areas);

  // Adds an association between `serialized_origin` and `map_id` into the
  // namespace `namespace_id`.
  void AddAreaToNamespace(const std::string& namespace_id,
                          const std::string& serialized_origin,
                          const std::string& map_id,
                          leveldb::WriteBatch* batch);

  // Helpers for deleting data for `namespace_id` and `serialized_origin`.
  bool DeleteAreaHelper(const std::string& namespace_id,
                        const std::string& serialized_origin,
                        leveldb::WriteBatch* batch);

  // Retrieves the map id for `namespace_id` and `serialized_origin`. It's not
  // an error if the map doesn't exist.
  bool GetMapForArea(const std::string& namespace_id,
                     const std::string& serialized_origin,
                     const leveldb::ReadOptions& options,
                     bool* exists,
                     std::string* map_id);

  // Creates a new map for `namespace_id` and `storage_key`. `map_id` will hold
  // the id of the created map. If there is a map for `namespace_id` and
  // `storage_key`, this just overwrites the map id. The caller is responsible
  // for decreasing the ref count.
  bool CreateMapForArea(const std::string& namespace_id,
                        const blink::StorageKey& storage_key,
                        std::string* map_id,
                        leveldb::WriteBatch* batch);
  // Reads the contents of the map `map_id` into `result`. If `only_keys` is
  // true, only keys are aread from the database and the values in `result` will
  // be empty.
  bool ReadMap(const std::string& map_id,
               const leveldb::ReadOptions& options,
               LegacyDomStorageValuesMap* result,
               bool only_keys);
  // Writes `values` into the map `map_id`.
  void WriteValuesToMap(const std::string& map_id,
                        const LegacyDomStorageValuesMap& values,
                        leveldb::WriteBatch* batch);

  bool GetMapRefCount(const std::string& map_id, int64_t* ref_count);
  bool IncreaseMapRefCount(const std::string& map_id,
                           leveldb::WriteBatch* batch);
  // Decreases the ref count of a map by `decrease`. If the ref count goes to 0,
  // deletes the map.
  bool DecreaseMapRefCount(const std::string& map_id,
                           int decrease,
                           leveldb::WriteBatch* batch);

  // Deletes all values in `map_id`.
  bool ClearMap(const std::string& map_id, leveldb::WriteBatch* batch);

  // Breaks the association between (`namespace_id`, `storage_key`) and `map_id`
  // and creates a new map for (`namespace_id`, `storage_key`). Copies the data
  // from the old map if `copy_data` is true.
  bool DeepCopyArea(const std::string& namespace_id,
                    const blink::StorageKey& storage_key,
                    bool copy_data,
                    std::string* map_id,
                    leveldb::WriteBatch* batch);

  // Helper functions for creating the keys needed for the schema.
  static std::string NamespaceStartKey(const std::string& namespace_id);
  static std::string NamespaceKey(const std::string& namespace_id,
                                  const std::string& serialized_origin);
  static const char* NamespacePrefix();
  static std::string MapRefCountKey(const std::string& map_id);
  static std::string MapKey(const std::string& map_id, const std::string& key);
  static const char* NextMapIdKey();

  std::unique_ptr<leveldb::DB> db_;
  base::FilePath file_path_;

  // For protecting the database opening code. Also guards the variables below.
  base::Lock db_lock_;

  // True if a database error has occurred (e.g., cannot read data).
  bool db_error_ GUARDED_BY(db_lock_);
  // True if the database is in an inconsistent state.
  bool is_inconsistent_ GUARDED_BY(db_lock_);
  // True if the database is in a failed or inconsistent state, and we have
  // already deleted it (as an attempt to recover later).
  bool invalid_db_deleted_ GUARDED_BY(db_lock_);

  // The number of database operations in progress. We need this so that we can
  // delete an inconsistent database at the right moment.
  int operation_count_ GUARDED_BY(db_lock_);

  // Used to check methods that run on the commit sequence.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TESTING_LEGACY_SESSION_STORAGE_DATABASE_H_
