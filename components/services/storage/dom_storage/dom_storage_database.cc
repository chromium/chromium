// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_database.h"

#include <algorithm>
#include <utility>

#include "base/debug/leak_annotations.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace storage {

namespace {

// IOError message returned whenever a call is made on a DomStorageDatabase
// which has been invalidated (e.g. by a failed |RewriteDB()| operation).
const char kInvalidDatabaseMessage[] = "DomStorageDatabase no longer valid.";

class DomStorageDatabaseEnv : public leveldb_env::ChromiumEnv {
 public:
  DomStorageDatabaseEnv()
      : ChromiumEnv("ChromiumEnv.StorageService", CreateFilesystemProxy()) {}

  DomStorageDatabaseEnv(const DomStorageDatabaseEnv&) = delete;
  DomStorageDatabaseEnv& operator=(const DomStorageDatabaseEnv&) = delete;
};

DomStorageDatabaseEnv* GetDomStorageDatabaseEnv() {
  static base::NoDestructor<DomStorageDatabaseEnv> env;
  return env.get();
}

std::string MakeFullPersistentDBName(const base::FilePath& directory,
                                     const std::string& db_name) {
  // ChromiumEnv treats DB name strings as UTF-8 file paths.
  return directory.Append(base::FilePath::FromUTF8Unsafe(db_name))
      .AsUTF8Unsafe();
}

leveldb_env::Options CreateDefaultInMemoryOptions() {
  leveldb_env::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;
  return options;
}

leveldb_env::Options AddEnvToOptions(const leveldb_env::Options& options,
                                     leveldb::Env* env) {
  leveldb_env::Options new_options = options;
  new_options.env = env;
  return new_options;
}

std::unique_ptr<leveldb::DB> TryOpenDB(
    const leveldb_env::Options& options,
    const std::string& name,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    DomStorageDatabase::StatusCallback callback) {
  std::unique_ptr<leveldb::DB> db;
  leveldb::Status status = leveldb_env::OpenDB(options, name, &db);
  callback_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), status));
  return db;
}

leveldb::Slice MakeSlice(base::span<const uint8_t> data) {
  if (data.empty())
    return leveldb::Slice();
  return leveldb::Slice(reinterpret_cast<const char*>(data.data()),
                        data.size());
}

DomStorageDatabase::KeyValuePair MakeKeyValuePair(const leveldb::Slice& key,
                                                  const leveldb::Slice& value) {
  auto key_span = base::make_span(key);
  auto value_span = base::make_span(value);
  return DomStorageDatabase::KeyValuePair(
      DomStorageDatabase::Key(key_span.begin(), key_span.end()),
      DomStorageDatabase::Value(value_span.begin(), value_span.end()));
}

template <typename Func>
DomStorageDatabase::Status ForEachWithPrefix(leveldb::DB* db,
                                             DomStorageDatabase::KeyView prefix,
                                             Func function) {
  // NOTE: We disable filling the cache for bulk scans. Either this is for
  // deletion (where caching doesn't make sense) or a mass-read, which the user
  // should be caching or only needing once.
  leveldb::ReadOptions options;
  options.fill_cache = false;
  std::unique_ptr<leveldb::Iterator> iter(db->NewIterator(options));
  const leveldb::Slice prefix_slice(MakeSlice(prefix));
  iter->Seek(prefix_slice);
  for (; iter->Valid(); iter->Next()) {
    if (!iter->key().starts_with(prefix_slice))
      break;
    function(iter->key(), iter->value());
  }
  return iter->status();
}

}  // namespace

DomStorageDatabase::KeyValuePair::KeyValuePair() = default;

DomStorageDatabase::KeyValuePair::~KeyValuePair() = default;

DomStorageDatabase::KeyValuePair::KeyValuePair(KeyValuePair&&) = default;

DomStorageDatabase::KeyValuePair::KeyValuePair(const KeyValuePair&) = default;

DomStorageDatabase::KeyValuePair::KeyValuePair(Key key, Value value)
    : key(std::move(key)), value(std::move(value)) {}

DomStorageDatabase::KeyValuePair& DomStorageDatabase::KeyValuePair::operator=(
    KeyValuePair&&) = default;

DomStorageDatabase::KeyValuePair& DomStorageDatabase::KeyValuePair::operator=(
    const KeyValuePair&) = default;

bool DomStorageDatabase::KeyValuePair::operator==(
    const KeyValuePair& rhs) const {
  return std::tie(key, value) == std::tie(rhs.key, rhs.value);
}

DomStorageDatabase::DomStorageDatabase(
    PassKey,
    const base::FilePath& directory,
    const std::string& name,
    const leveldb_env::Options& options,
    const absl::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    StatusCallback callback)
    : DomStorageDatabase(PassKey(),
                         MakeFullPersistentDBName(directory, name),
                         /*env=*/nullptr,
                         options,
                         memory_dump_id,
                         std::move(callback_task_runner),
                         std::move(callback)) {}

DomStorageDatabase::DomStorageDatabase(
    PassKey,
    const std::string& tracking_name,
    const absl::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    StatusCallback callback)
    : DomStorageDatabase(PassKey(),
                         "",
                         leveldb_chrome::NewMemEnv(tracking_name),
                         CreateDefaultInMemoryOptions(),
                         memory_dump_id,
                         std::move(callback_task_runner),
                         std::move(callback)) {}

DomStorageDatabase::DomStorageDatabase(
    PassKey,
    const std::string& name,
    std::unique_ptr<leveldb::Env> env,
    const leveldb_env::Options& options,
    const absl::optional<base::trace_event::MemoryAllocatorDumpGuid>
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    StatusCallback callback)
    : name_(name),
      env_(std::move(env)),
      options_(AddEnvToOptions(options,
                               env_ ? env_.get() : GetDomStorageDatabaseEnv())),
      memory_dump_id_(memory_dump_id),
      db_(TryOpenDB(options_,
                    name,
                    std::move(callback_task_runner),
                    std::move(callback))) {
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "MojoLevelDB", base::SequencedTaskRunner::GetCurrentDefault(),
          MemoryDumpProvider::Options());
}

template <typename... Args>
void DomStorageDatabase::CreateSequenceBoundDomStorageDatabase(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    DomStorageDatabase::OpenCallback callback,
    Args&&... args) {
  auto database = std::make_unique<base::SequenceBound<DomStorageDatabase>>();

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
  *database_ptr = base::SequenceBound<DomStorageDatabase>(
      blocking_task_runner, PassKey(), args...,
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          [](base::SequenceBound<DomStorageDatabase>* database_ptr,
             DomStorageDatabase::OpenCallback callback,
             leveldb::Status status) {
            auto database = base::WrapUnique(database_ptr);
            if (status.ok())
              std::move(callback).Run(std::move(*database), status);
            else
              std::move(callback).Run({}, status);
          },
          database_ptr, std::move(callback)));
}

DomStorageDatabase::~DomStorageDatabase() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  if (destruction_callback_)
    std::move(destruction_callback_).Run();
}

// static
void DomStorageDatabase::OpenDirectory(
    const base::FilePath& directory,
    const std::string& name,
    const leveldb_env::Options& options,
    const absl::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OpenCallback callback) {
  DCHECK(directory.IsAbsolute());
  CreateSequenceBoundDomStorageDatabase(std::move(blocking_task_runner),
                                        std::move(callback), directory, name,
                                        options, memory_dump_id);
}

// static
void DomStorageDatabase::OpenInMemory(
    const std::string& name,
    const absl::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OpenCallback callback) {
  CreateSequenceBoundDomStorageDatabase(std::move(blocking_task_runner),
                                        std::move(callback), name,
                                        memory_dump_id);
}

// static
void DomStorageDatabase::Destroy(
    const base::FilePath& directory,
    const std::string& name,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    StatusCallback callback) {
  blocking_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const std::string& db_name,
             scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
             StatusCallback callback) {
            leveldb_env::Options options;
            options.env = GetDomStorageDatabaseEnv();
            callback_task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               leveldb::DestroyDB(db_name, options)));
          },
          MakeFullPersistentDBName(directory, name),
          base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback)));
}

DomStorageDatabase::Status DomStorageDatabase::Get(KeyView key,
                                                   Value* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
  std::string value;
  Status status = db_->Get(leveldb::ReadOptions(), MakeSlice(key), &value);
  *out_value = Value(value.begin(), value.end());
  return status;
}

DomStorageDatabase::Status DomStorageDatabase::Put(KeyView key,
                                                   ValueView value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
  return db_->Put(leveldb::WriteOptions(), MakeSlice(key), MakeSlice(value));
}

DomStorageDatabase::Status DomStorageDatabase::Delete(KeyView key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
  return db_->Delete(leveldb::WriteOptions(), MakeSlice(key));
}

DomStorageDatabase::Status DomStorageDatabase::GetPrefixed(
    KeyView prefix,
    std::vector<KeyValuePair>* entries) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
  return ForEachWithPrefix(
      db_.get(), prefix,
      [&](const leveldb::Slice& key, const leveldb::Slice& value) {
        entries->push_back(MakeKeyValuePair(key, value));
      });
}

DomStorageDatabase::Status DomStorageDatabase::DeletePrefixed(
    KeyView prefix,
    leveldb::WriteBatch* batch) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
  Status status = ForEachWithPrefix(
      db_.get(), prefix,
      [&](const leveldb::Slice& key, const leveldb::Slice& value) {
        batch->Delete(key);
      });
  return status;
}

DomStorageDatabase::Status DomStorageDatabase::CopyPrefixed(
    KeyView prefix,
    KeyView new_prefix,
    leveldb::WriteBatch* batch) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
  Key new_key(new_prefix.begin(), new_prefix.end());
  Status status = ForEachWithPrefix(
      db_.get(), prefix,
      [&](const leveldb::Slice& key, const leveldb::Slice& value) {
        DCHECK_GE(key.size(), prefix.size());  // By definition.
        size_t suffix_length = key.size() - prefix.size();
        new_key.resize(new_prefix.size() + suffix_length);
        std::copy(key.data() + prefix.size(), key.data() + key.size(),
                  new_key.begin() + new_prefix.size());
        batch->Put(MakeSlice(new_key), value);
      });
  return status;
}

DomStorageDatabase::Status DomStorageDatabase::Commit(
    leveldb::WriteBatch* batch) const {
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
  if (fail_commits_for_testing_)
    return Status::IOError("Simulated I/O Error");
  return db_->Write(leveldb::WriteOptions(), batch);
}

DomStorageDatabase::Status DomStorageDatabase::RewriteDB() {
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
  Status status = leveldb_env::RewriteDB(options_, name_, &db_);
  if (!status.ok())
    db_.reset();
  return status;
}

bool DomStorageDatabase::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  auto* dump = leveldb_env::DBTracker::GetOrCreateAllocatorDump(pmd, db_.get());
  if (!dump)
    return true;
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
