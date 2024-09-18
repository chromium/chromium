// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_BACKEND_H_
#define COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_BACKEND_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/model/data_type_store.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {
class DB;
class Env;
class WriteBatch;
}  // namespace leveldb

namespace syncer {

// DataTypeStoreBackend handles operations with leveldb. It is oblivious of the
// fact that it is called from separate thread (with the exception of ctor),
// meaning it shouldn't deal with callbacks and task_runners.
//
// Created and destroyed on any sequence, but otherwise initialized and used on
// a single sequence (attached during Init()).
class DataTypeStoreBackend
    : public base::RefCountedThreadSafe<DataTypeStoreBackend> {
 public:
  static scoped_refptr<DataTypeStoreBackend> CreateInMemoryForTest();

  // Creates a new and uninitialized instance of DataTypeStoreBackend. Init()
  // must be called afterwards, which binds the instance to a certain sequence.
  static scoped_refptr<DataTypeStoreBackend> CreateUninitialized();

  DataTypeStoreBackend(const DataTypeStoreBackend&) = delete;
  DataTypeStoreBackend& operator=(const DataTypeStoreBackend&) = delete;

  // Opens the database at `path`, creating it if it doesn't exist yet. May be
  // called from a sequence that is different to the constructing one, but from
  // this point on the backend is bound to the current sequence, and must be
  // used on it. It may be destructed on any sequence though.
  // An ("old-" -> "new-") entry in `prefixes_to_update_or_delete` will cause
  // all records [old-key, value] to be updated to [new-key, value].
  // An ("old-" -> std::nullopt) entry in `prefixes_to_update_or_delete` will
  // cause all records [old-key, value] to be deleted.
  std::optional<ModelError> Init(
      const base::FilePath& path,
      const base::flat_map<std::string, std::optional<std::string>>&
          prefixes_to_update_or_delete);

  // Can be called from any sequence.
  bool IsInitialized() const;

  // Reads records with keys formed by prepending ids from |id_list| with
  // |prefix|. If the record is found its id (without prefix) and value is
  // appended to record_list. If record is not found its id is appended to
  // |missing_id_list|. It is not an error that records for ids are not found so
  // function will still return success in this case.
  std::optional<ModelError> ReadRecordsWithPrefix(
      const std::string& prefix,
      const DataTypeStore::IdList& id_list,
      DataTypeStore::RecordList* record_list,
      DataTypeStore::IdList* missing_id_list);

  // Reads all records with keys starting with |prefix|. Prefix is removed from
  // key before it is added to |record_list|.
  std::optional<ModelError> ReadAllRecordsWithPrefix(
      const std::string& prefix,
      DataTypeStore::RecordList* record_list);

  // Writes modifications accumulated in |write_batch| to database.
  std::optional<ModelError> WriteModifications(
      std::unique_ptr<leveldb::WriteBatch> write_batch);

  std::optional<ModelError> DeleteDataAndMetadataForPrefix(
      const std::string& prefix);

  // Migrate the db schema from |current_version| to |desired_version|.
  std::optional<ModelError> MigrateForTest(int64_t current_version,
                                           int64_t desired_version);

  // Attempts to read and return the database's version.
  int64_t GetStoreVersionForTest();

  // Some constants exposed for testing.
  static const int64_t kLatestSchemaVersion;
  static const char kDBSchemaDescriptorRecordId[];

 private:
  friend class base::RefCountedThreadSafe<DataTypeStoreBackend>;

  // This is a slightly adapted version of base::OnTaskRunnerDeleter: The one
  // difference is that if the destruction request already happens on the target
  // sequence, then this avoids posting a task, and instead deletes the given
  // object immediately. This is convenient for unit-tests that don't run all
  // posted tasks, to avoid leaking memory.
  struct CustomOnTaskRunnerDeleter {
    explicit CustomOnTaskRunnerDeleter(
        scoped_refptr<base::SequencedTaskRunner> task_runner);
    CustomOnTaskRunnerDeleter(CustomOnTaskRunnerDeleter&&);
    CustomOnTaskRunnerDeleter& operator=(CustomOnTaskRunnerDeleter&&);
    ~CustomOnTaskRunnerDeleter();

    // For compatibility with std:: deleters.
    template <typename T>
    void operator()(const T* ptr) {
      if (!ptr) {
        return;
      }

      if (task_runner_->RunsTasksInCurrentSequence()) {
        delete ptr;
      } else {
        task_runner_->DeleteSoon(FROM_HERE, ptr);
      }
    }

    scoped_refptr<base::SequencedTaskRunner> task_runner_;
  };

  // Normally |env| should be nullptr, this causes leveldb to use default disk
  // based environment from leveldb::Env::Default().
  // Providing |env| allows to override environment used by leveldb for tests
  // with in-memory or faulty environment.
  explicit DataTypeStoreBackend(std::unique_ptr<leveldb::Env> env);

  ~DataTypeStoreBackend();

  // Opens leveldb database passing correct options. On success sets |db_| and
  // returns ok status. On failure |db_| is nullptr and returned status reflects
  // failure type.
  leveldb::Status OpenDatabase(const std::string& path, leveldb::Env* env);

  // Destroys leveldb database. Used for recovering after database corruption.
  leveldb::Status DestroyDatabase(const std::string& path, leveldb::Env* env);

  // Attempts to read and return the database's version.
  // If there is not a schema descriptor present, the value returned is 0.
  // If an error occurs, the value returned is kInvalidSchemaVersion(-1).
  int64_t GetStoreVersion();

  // Migrate the db schema from |current_version| to |desired_version|,
  // returning nullopt on success.
  std::optional<ModelError> Migrate(int64_t current_version,
                                    int64_t desired_version);

  // Migrates from no version record at all (version 0) to version 1 of
  // the schema, returning true on success.
  bool Migrate0To1();

  // For all records whose keys start with `old_prefix`, updates their keys to
  // start with `new_prefix` instead. Can be used to move data between
  // StorageType::kUnspecified and StorageType::kAccount.
  std::optional<ModelError> UpdateDataPrefix(const std::string& old_prefix,
                                             const std::string& new_prefix);

  // Removes all records starting with `prefix`.
  std::optional<ModelError> RemoveDataPrefix(const std::string& prefix);

  // In some scenarios DataTypeStoreBackend holds ownership of env. Typical
  // example is when test creates in memory environment with CreateInMemoryEnv
  // and wants it to be destroyed along with backend. This is achieved by
  // passing ownership of env to TakeEnvOwnership function.
  //
  // env_ declaration should appear before declaration of db_ because
  // environment object should still be valid when db_'s destructor is called.
  //
  // Note that no custom deleter is used for |env_| because it is non-null for
  // callers of CreateInMemoryForTest(), which initializes |db_| synchronously
  // and hence |db_| also gets deleted without involving task-posting (i.e.
  // |db_| cannot outlive |env_|).
  const std::unique_ptr<leveldb::Env> env_;

  // Destruction of |leveldb::DB| may incur blocking calls, and this class may
  // be destructed on any sequence, so let's avoid worst-case blocking the UI
  // thread by destroying leveldb::DB on the sequence where Init() was called.
  std::unique_ptr<leveldb::DB, CustomOnTaskRunnerDeleter> db_;

  // Ensures that operations with backend are performed seqentially, not
  // concurrently.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_BACKEND_H_
