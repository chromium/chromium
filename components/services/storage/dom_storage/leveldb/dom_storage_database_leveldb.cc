// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/types/expected_macros.h"
#include "base/types/pass_key.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb_utils.h"
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

leveldb_env::Options MakeOnDiskOptions() {
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
    const base::FilePath& directory,
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id)
    : memory_dump_id_(memory_dump_id) {
  const bool is_in_memory = directory.empty();
  if (is_in_memory) {
    env_ = leveldb_chrome::NewMemEnv(name);
    options_.env = env_.get();
  } else {
    CHECK(directory.IsAbsolute());
    name_ = MakeFullPersistentDBName(directory, name);
    options_ = MakeOnDiskOptions();
  }
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "MojoLevelDB", base::SequencedTaskRunner::GetCurrentDefault(),
          MemoryDumpProvider::Options());
}

DbStatus DomStorageDatabaseLevelDB::InitializeLevelDB() {
  leveldb::Status status = leveldb_env::OpenDB(options_, name_, &db_);
  return FromLevelDBStatus(status);
}

DomStorageDatabaseLevelDB::~DomStorageDatabaseLevelDB() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  if (destruction_callback_) {
    std::move(destruction_callback_).Run();
  }
}

// static
StatusOr<std::unique_ptr<DomStorageDatabaseLevelDB>>
DomStorageDatabaseLevelDB::Open(
    const base::FilePath& directory,
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    KeyView version_key,
    int64_t min_supported_version,
    int64_t max_supported_version) {
  std::unique_ptr<DomStorageDatabaseLevelDB> instance = base::WrapUnique(
      new DomStorageDatabaseLevelDB(directory, name, memory_dump_id));
  DbStatus status = instance->InitializeLevelDB();
  if (!status.ok()) {
    return base::unexpected(std::move(status));
  }

  status = instance->EnsureVersion(version_key, min_supported_version,
                                   max_supported_version);
  if (!status.ok()) {
    return base::unexpected(std::move(status));
  }
  return instance;
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
            std::move(callback).Run(FromLevelDBStatus(
                leveldb::DestroyDB(db_name, MakeOnDiskOptions())));
          },
          MakeFullPersistentDBName(directory, name),
          base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                             std::move(callback))));
}

StatusOr<DomStorageDatabase::Value> DomStorageDatabaseLevelDB::Get(
    KeyView key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return base::unexpected(DbStatus::IOError(kInvalidDatabaseMessage));
  }
  std::string value;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), MakeSlice(key), &value);
  if (!status.ok()) {
    return base::unexpected(FromLevelDBStatus(status));
  }
  return Value(value.begin(), value.end());
}

DbStatus DomStorageDatabaseLevelDB::Put(KeyView key, ValueView value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return DbStatus::IOError(kInvalidDatabaseMessage);
  }
  return FromLevelDBStatus(
      db_->Put(leveldb::WriteOptions(), MakeSlice(key), MakeSlice(value)));
}

StatusOr<std::vector<DomStorageDatabase::KeyValuePair>>
DomStorageDatabaseLevelDB::GetPrefixed(KeyView prefix) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return base::unexpected(DbStatus::IOError(kInvalidDatabaseMessage));
  }

  std::vector<DomStorageDatabase::KeyValuePair> entries;
  DbStatus status = ForEachWithPrefix(
      db_.get(), prefix,
      [&](const leveldb::Slice& key, const leveldb::Slice& value) {
        entries.push_back(MakeKeyValuePair(key, value));
      });
  if (!status.ok()) {
    return base::unexpected(std::move(status));
  }

  return entries;
}

std::unique_ptr<DomStorageBatchOperationLevelDB>
DomStorageDatabaseLevelDB::CreateBatchOperation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<DomStorageBatchOperationLevelDB>(
      weak_factory_.GetWeakPtr());
}

DbStatus DomStorageDatabaseLevelDB::RewriteDB() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return DbStatus::IOError(kInvalidDatabaseMessage);
  }
  leveldb::Status status = leveldb_env::RewriteDB(options_, name_, &db_);
  if (!status.ok()) {
    db_.reset();
  }
  return FromLevelDBStatus(status);
}

bool DomStorageDatabaseLevelDB::ShouldFailAllCommitsForTesting() {
  return fail_all_commits_for_testing_;
}

void DomStorageDatabaseLevelDB::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {
  destruction_callback_ = std::move(callback);
}

void DomStorageDatabaseLevelDB::MakeAllCommitsFailForTesting() {
  fail_all_commits_for_testing_ = true;
}

// This can only be called from `DomStorageBatchOperationLevelDB`.
leveldb::DB* DomStorageDatabaseLevelDB::GetLevelDBDatabase(
    base::PassKey<DomStorageBatchOperationLevelDB> key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_.get();
}
DbStatus DomStorageDatabaseLevelDB::EnsureVersion(
    KeyView version_key,
    int64_t min_supported_version,
    int64_t max_supported_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  StatusOr<Value> version_bytes = Get(version_key);
  if (!version_bytes.has_value()) {
    if (version_bytes.error().IsNotFound()) {
      // Write the version entry when it does not exist.
      return Put(version_key, base::as_byte_span(
                                  base::NumberToString(max_supported_version)));
    }
    // The database failed to read the version key.
    return version_bytes.error();
  }

  // Verify the contents of the version key.
  int64_t actual_version;
  if (!base::StringToInt64(base::as_string_view(*version_bytes),
                           &actual_version)) {
    return DbStatus::Corruption("version is not a number");
  }

  if (actual_version < min_supported_version ||
      actual_version > max_supported_version) {
    return DbStatus::Corruption("version is unsupported");
  }
  return DbStatus::OK();
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
