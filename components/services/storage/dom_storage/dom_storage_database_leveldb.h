// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_LEVELDB_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_LEVELDB_H_

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

// A DomStorageDatabase implementation that uses LevelDB to store data. This
// object is not thread-safe. Additionally, it must be instantiated on a
// sequence that allows blocking file operations.
class DomStorageDatabaseLevelDB
    : public DomStorageDatabase,
      private base::trace_event::MemoryDumpProvider {
 private:
  using PassKey = base::PassKey<DomStorageDatabaseLevelDB>;

 public:
  // Callback used for basic async operations on this class.
  using StatusCallback = base::OnceCallback<void(DbStatus)>;

  // Use the static factory functions in DomStorageDatabase to construct this
  // class. These constructors are only public for the sake of
  // `base::SequenceBound`.
  DomStorageDatabaseLevelDB(
      PassKey,
      const base::FilePath& directory,
      const std::string& name,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      StatusCallback callback);
  DomStorageDatabaseLevelDB(
      PassKey,
      const std::string& tracking_name,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      StatusCallback callback);
  DomStorageDatabaseLevelDB(const DomStorageDatabaseLevelDB&) = delete;
  DomStorageDatabaseLevelDB& operator=(const DomStorageDatabaseLevelDB&) =
      delete;
  ~DomStorageDatabaseLevelDB() override;

  // DomStorageDatabase implementation:
  DbStatus Get(KeyView key, Value* out_value) const override;
  DbStatus Put(KeyView key, ValueView value) const override;
  DbStatus GetPrefixed(KeyView prefix,
                       std::vector<KeyValuePair>* entries) const override;
  DbStatus RewriteDB() override;
  std::unique_ptr<DomStorageBatchOperation> CreateBatchOperation() override;
  bool ShouldFailAllCommits() const override;
  void SetDestructionCallbackForTesting(base::OnceClosure callback) override;
  void MakeAllCommitsFailForTesting() override;

  // This can only be called from `DomStorageBatchOperationLevelDB`.
  leveldb::DB* GetLevelDBDatabase(
      base::PassKey<DomStorageBatchOperationLevelDB> key) const;

 private:
  friend class DomStorageDatabaseFactory;
  // Initializes a new DomStorageDatabaseLevelDB, creating or opening persistent
  // on-filesystem database as specified. Asynchronously invokes `callback` when
  // done.
  //
  // This must be called on a sequence that allows blocking operations.
  void Init(StatusCallback callback);

  template <typename... Args>
  static void CreateSequenceBoundDomStorageDatabase(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      DomStorageDatabaseFactory::OpenCallback callback,
      Args&&... args);

  static void OpenDirectory(
      const base::FilePath& directory,
      const std::string& name,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      DomStorageDatabaseFactory::OpenCallback callback);

  static void OpenInMemory(
      const std::string& name,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      DomStorageDatabaseFactory::OpenCallback callback);

  static void Destroy(
      const base::FilePath& directory,
      const std::string& name,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      StatusCallback callback);

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  const std::string name_;
  const std::unique_ptr<leveldb::Env> env_;
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

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_LEVELDB_H_
