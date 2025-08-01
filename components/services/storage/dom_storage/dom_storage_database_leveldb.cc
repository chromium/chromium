// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_database_leveldb.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/types/pass_key.h"
#include "components/services/storage/dom_storage/dom_storage_batch_operation_leveldb.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database_leveldb_utils.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "storage/common/database/leveldb_status_helper.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace storage {

namespace {

class DomStorageDatabaseLevelDBEnv : public leveldb_env::ChromiumEnv {
 public:
  DomStorageDatabaseLevelDBEnv() : ChromiumEnv(CreateFilesystemProxy()) {}

  DomStorageDatabaseLevelDBEnv(const DomStorageDatabaseLevelDBEnv&) = delete;
  DomStorageDatabaseLevelDBEnv& operator=(const DomStorageDatabaseLevelDBEnv&) =
      delete;
};

std::string MakeFullPersistentDBName(const base::FilePath& directory,
                                     const std::string& db_name) {
  // ChromiumEnv treats DB name strings as UTF-8 file paths.
  return directory.Append(base::FilePath::FromUTF8Unsafe(db_name))
      .AsUTF8Unsafe();
}

// Used for disk DBs.
leveldb_env::Options MakeOptions() {
  leveldb_env::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // use minimum
  // Default write_buffer_size is 4 MB but that might leave a 3.999
  // memory allocation in RAM from a log file recovery.
  options.write_buffer_size = 64 * 1024;

  // We disable caching because all reads are one-offs such as in
  // `LocalStorageImpl::OnDatabaseOpened()`, or they are bulk scans (as in
  // `ForEachWithPrefix`). In the case of bulk scans, they're either for
  // deletion (where caching doesn't make sense) or a mass-read, which we cache
  // in memory.
  options.block_cache = leveldb_chrome::GetSharedInMemoryBlockCache();

  static base::NoDestructor<DomStorageDatabaseLevelDBEnv> env;
  options.env = env.get();
  return options;
}

std::unique_ptr<leveldb::DB> TryOpenDB(
    const leveldb_env::Options& options,
    const std::string& name,
    DomStorageDatabaseLevelDB::StatusCallback callback) {
  std::unique_ptr<leveldb::DB> db;
  leveldb::Status status = leveldb_env::OpenDB(options, name, &db);
  std::move(callback).Run(FromLevelDBStatus(status));
  return db;
}

DomStorageDatabase::KeyValuePair MakeKeyValuePair(const leveldb::Slice& key,
                                                  const leveldb::Slice& value) {
  base::span key_span(key);
  base::span value_span(value);
  return DomStorageDatabase::KeyValuePair(
      DomStorageDatabase::Key(key_span.begin(), key_span.end()),
      DomStorageDatabase::Value(value_span.begin(), value_span.end()));
}

}  // namespace

DomStorageDatabaseLevelDB::DomStorageDatabaseLevelDB(
    PassKey,
    const base::FilePath& directory,
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    StatusCallback callback)
    : name_(MakeFullPersistentDBName(directory, name)),
      options_(MakeOptions()),
      memory_dump_id_(memory_dump_id) {
  Init(std::move(callback));
}

DomStorageDatabaseLevelDB::DomStorageDatabaseLevelDB(
    PassKey,
    const std::string& tracking_name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    StatusCallback callback)
    : env_(leveldb_chrome::NewMemEnv(tracking_name)),
      memory_dump_id_(memory_dump_id) {
  options_.env = env_.get();
  Init(std::move(callback));
}

void DomStorageDatabaseLevelDB::Init(
    StatusCallback callback) {
  db_ = TryOpenDB(options_, name_, std::move(callback));
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "MojoLevelDB", base::SequencedTaskRunner::GetCurrentDefault(),
          MemoryDumpProvider::Options());
}

template <typename... Args>
void DomStorageDatabaseLevelDB::CreateSequenceBoundDomStorageDatabase(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    DomStorageDatabaseFactory::OpenCallback callback,
    Args&&... args) {
  auto database =
      std::make_unique<base::SequenceBound<DomStorageDatabaseLevelDB>>();

  // Subtle: We bind `database` as an unmanaged pointer during the async opening
  // operation so that it leaks in case the bound callback below never gets a
  // chance to run (because scheduler shutdown happens first).
  //
  // This is because the callback below is posted to
  // SequencedTaskRunner::GetCurrentDefault(), which may not itself be
  // shutdown-blocking; so if shutdown completes before the task runs, the
  // callback below is destroyed along with any of its owned arguments.
  // Meanwhile, SequenceBound destruction posts a task to its bound TaskRunner,
  // which in this case is one which runs shutdown-blocking tasks.
  //
  // The net result of all of this is that if the SequenceBound were an owned
  // argument, it might attempt to post a shutdown-blocking task after shutdown
  // has completed, which is not allowed and will DCHECK. Leaving the object
  // temporarily unmanaged during this window of potential failure avoids such a
  // DCHECK, and if shutdown does not happen during that window, the object's
  // ownership will finally be left to the caller's discretion.
  //
  // See https://crbug.com/1174179.
  auto* database_ptr = database.release();
  ANNOTATE_LEAKING_OBJECT_PTR(database_ptr);
  *database_ptr = base::SequenceBound<DomStorageDatabaseLevelDB>(
      blocking_task_runner, PassKey(), args...,
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(
              [](base::SequenceBound<DomStorageDatabaseLevelDB>* database_ptr,
                 DomStorageDatabaseFactory::OpenCallback callback,
                 DbStatus status) {
                auto database = base::WrapUnique(database_ptr);
                if (status.ok()) {
                  std::move(callback).Run(std::move(*database), status);
                } else {
                  std::move(callback).Run({}, status);
                }
              },
              database_ptr, std::move(callback))));
}
DomStorageDatabaseLevelDB::~DomStorageDatabaseLevelDB() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  if (destruction_callback_) {
    std::move(destruction_callback_).Run();
  }
}

// static
void DomStorageDatabaseLevelDB::OpenDirectory(
    const base::FilePath& directory,
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    DomStorageDatabaseFactory::OpenCallback callback) {
  DCHECK(directory.IsAbsolute());
  CreateSequenceBoundDomStorageDatabase(std::move(blocking_task_runner),
                                        std::move(callback), directory, name,
                                        memory_dump_id);
}

// static
void DomStorageDatabaseLevelDB::OpenInMemory(
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    DomStorageDatabaseFactory::OpenCallback callback) {
  CreateSequenceBoundDomStorageDatabase(std::move(blocking_task_runner),
                                        std::move(callback), name,
                                        memory_dump_id);
}

// static
void DomStorageDatabaseLevelDB::Destroy(
    const base::FilePath& directory,
    const std::string& name,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    StatusCallback callback) {
  blocking_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const std::string& db_name, StatusCallback callback) {
            std::move(callback).Run(
                FromLevelDBStatus(leveldb::DestroyDB(db_name, MakeOptions())));
          },
          MakeFullPersistentDBName(directory, name),
          base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                             std::move(callback))));
}

DbStatus DomStorageDatabaseLevelDB::Get(KeyView key, Value* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return DbStatus::IOError(kInvalidDatabaseMessage);
  }
  std::string value;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), MakeSlice(key), &value);
  *out_value = Value(value.begin(), value.end());
  return FromLevelDBStatus(status);
}

DbStatus DomStorageDatabaseLevelDB::Put(KeyView key, ValueView value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return DbStatus::IOError(kInvalidDatabaseMessage);
  }
  return FromLevelDBStatus(
      db_->Put(leveldb::WriteOptions(), MakeSlice(key), MakeSlice(value)));
}

DbStatus DomStorageDatabaseLevelDB::GetPrefixed(
    KeyView prefix,
    std::vector<KeyValuePair>* entries) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return DbStatus::IOError(kInvalidDatabaseMessage);
  }
  return ForEachWithPrefix(
      db_.get(), prefix,
      [&](const leveldb::Slice& key, const leveldb::Slice& value) {
        entries->push_back(MakeKeyValuePair(key, value));
      });
}

DbStatus DomStorageDatabaseLevelDB::RewriteDB() {
  if (!db_) {
    return DbStatus::IOError(kInvalidDatabaseMessage);
  }
  leveldb::Status status = leveldb_env::RewriteDB(options_, name_, &db_);
  if (!status.ok()) {
    db_.reset();
  }
  return FromLevelDBStatus(status);
}

std::unique_ptr<DomStorageBatchOperation>
DomStorageDatabaseLevelDB::CreateBatchOperation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<DomStorageBatchOperationLevelDB>(
      weak_factory_.GetWeakPtr());
}

bool DomStorageDatabaseLevelDB::ShouldFailAllCommits() const {
  return fail_all_commits_;
}

void DomStorageDatabaseLevelDB::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {
  destruction_callback_ = std::move(callback);
}

void DomStorageDatabaseLevelDB::MakeAllCommitsFailForTesting() {
  fail_all_commits_ = true;
}

// This can only be called from `DomStorageBatchOperationLevelDB`.
leveldb::DB* DomStorageDatabaseLevelDB::GetLevelDBDatabase(
    base::PassKey<DomStorageBatchOperationLevelDB> key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_.get();
}

bool DomStorageDatabaseLevelDB::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs&,
    base::trace_event::ProcessMemoryDump* pmd) {
  auto* dump = leveldb_env::DBTracker::GetOrCreateAllocatorDump(pmd, db_.get());
  if (!dump) {
    return true;
  }
  auto* global_dump = pmd->CreateSharedGlobalAllocatorDump(*memory_dump_id_);
  pmd->AddOwnershipEdge(global_dump->guid(), dump->guid());
  // Add size to global dump to propagate the size of the database to the
  // client's dump.
  global_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                         base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                         dump->GetSizeInternal());
  return true;
}

}  // namespace storage
