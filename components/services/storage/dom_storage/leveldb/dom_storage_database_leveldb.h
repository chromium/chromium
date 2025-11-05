// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_DOM_STORAGE_DATABASE_LEVELDB_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_DOM_STORAGE_DATABASE_LEVELDB_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/types/pass_key.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "storage/common/database/db_status.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace base {
class FilePath;
}  // namespace base

namespace leveldb {
class DB;
class Env;
}  // namespace leveldb

namespace storage {

class DomStorageBatchOperationLevelDB;

// Wraps `leveldb::DB`, adding convenience functions that support both session
// storage and local storage. Utilities include:
// - Supporting prefix queries using an iterator to return a vector of results.
// - Converting all `leveldb::Status` to `DbStatus`.
// - Adding test hooks to simulate error conditions.
// - Providing memory dump support.
class DomStorageDatabaseLevelDB
    : private base::trace_event::MemoryDumpProvider {
 public:
  using Key = DomStorageDatabase::Key;
  using KeyView = DomStorageDatabase::KeyView;
  using Value = DomStorageDatabase::Value;
  using ValueView = DomStorageDatabase::ValueView;
  using KeyValuePair = DomStorageDatabase::KeyValuePair;

  DomStorageDatabaseLevelDB(const DomStorageDatabaseLevelDB&) = delete;
  DomStorageDatabaseLevelDB& operator=(const DomStorageDatabaseLevelDB&) =
      delete;
  ~DomStorageDatabaseLevelDB() override;

  DbStatus Get(KeyView key, Value* out_value) const;
  DbStatus Put(KeyView key, ValueView value);
  DbStatus GetPrefixed(KeyView prefix,
                       std::vector<KeyValuePair>* entries) const;
  std::unique_ptr<DomStorageBatchOperationLevelDB> CreateBatchOperation();

  DbStatus RewriteDB();
  bool ShouldFailAllCommits();
  void SetDestructionCallbackForTesting(base::OnceClosure callback);
  void MakeAllCommitsFailForTesting();

  // This can only be called from `DomStorageBatchOperationLevelDB`.
  leveldb::DB* GetLevelDBDatabase(
      base::PassKey<DomStorageBatchOperationLevelDB> key) const;

  // To create an in-memory database, provide an empty `directory`.
  static StatusOr<std::unique_ptr<DomStorageDatabaseLevelDB>> Open(
      const base::FilePath& directory,
      const std::string& name,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id);

  using StatusCallback = base::OnceCallback<void(DbStatus)>;

  static void Destroy(
      const base::FilePath& directory,
      const std::string& name,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      StatusCallback callback);

 private:
  DomStorageDatabaseLevelDB(
      const base::FilePath& directory,
      const std::string& name,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id);

  // Opens `db_` using `options_` and `name_` then returns the result.
  DbStatus InitializeLevelDB();

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  std::string name_;
  std::unique_ptr<leveldb::Env> env_;
  leveldb_env::Options options_;
  const std::optional<base::trace_event::MemoryAllocatorDumpGuid>
      memory_dump_id_;
  std::unique_ptr<leveldb::DB> db_;

  // If true, all calls to `Commit()` fail with an IOError. This should only be
  // set in tests to simulate disk failures.
  bool fail_all_commits_ = false;

  // Callback to run on destruction in tests.
  base::OnceClosure destruction_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DomStorageDatabaseLevelDB> weak_factory_{this};
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_DOM_STORAGE_DATABASE_LEVELDB_H_
