// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/local_storage_impl.h"

#include <inttypes.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/local_storage_database.pb.h"
#include "components/services/storage/dom_storage/storage_area_impl.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/public/cpp/constants.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

namespace storage {

// LevelDB database schema
// =======================
//
// Version 1 (in sorted order):
//   key: "VERSION"
//   value: "1"
//
//   key: "META:" + <StorageKey 'storage_key'>
//   value: <LocalStorageStorageKeyMetaData serialized as a string>
//
//   key: "_" + <StorageKey 'storage_key'> + '\x00' + <script controlled key>
//   value: <script controlled value>
//
// Note: The StorageKeys are serialized as origins, not URLs, i.e. with no
// trailing slashes.

namespace {

// Temporary alias as this code moves incrementally into the storage namespace.
using StorageAreaImpl = StorageAreaImpl;

constexpr base::StringPiece kVersionKey = "VERSION";
const uint8_t kMetaPrefix[] = {'M', 'E', 'T', 'A', ':'};
const int64_t kMinSchemaVersion = 1;
const int64_t kCurrentLocalStorageSchemaVersion = 1;

// After this many consecutive commit errors we'll throw away the entire
// database.
const int kCommitErrorThreshold = 8;

// Limits on the cache size and number of areas in memory, over which the areas
// are purged.
#if BUILDFLAG(IS_ANDROID)
const unsigned kMaxLocalStorageAreaCount = 10;
const size_t kMaxLocalStorageCacheSize = 2 * 1024 * 1024;
#else
const unsigned kMaxLocalStorageAreaCount = 50;
const size_t kMaxLocalStorageCacheSize = 20 * 1024 * 1024;
#endif

DomStorageDatabase::Key CreateMetaDataKey(
    const blink::StorageKey& storage_key) {
  std::string storage_key_str = storage_key.SerializeForLocalStorage();
  std::vector<uint8_t> serialized_storage_key(storage_key_str.begin(),
                                              storage_key_str.end());
  DomStorageDatabase::Key key;
  key.reserve(std::size(kMetaPrefix) + serialized_storage_key.size());
  key.insert(key.end(), kMetaPrefix, kMetaPrefix + std::size(kMetaPrefix));
  key.insert(key.end(), serialized_storage_key.begin(),
             serialized_storage_key.end());
  return key;
}

absl::optional<blink::StorageKey> ExtractStorageKeyFromMetaDataKey(
    const DomStorageDatabase::Key& key) {
  DCHECK_GT(key.size(), std::size(kMetaPrefix));
  const base::StringPiece key_string(reinterpret_cast<const char*>(key.data()),
                                     key.size());
  return blink::StorageKey::Deserialize(
      key_string.substr(std::size(kMetaPrefix)));
}

void SuccessResponse(base::OnceClosure callback, bool success) {
  std::move(callback).Run();
}

void IgnoreStatus(base::OnceClosure callback, leveldb::Status status) {
  std::move(callback).Run();
}

DomStorageDatabase::Key MakeStorageKeyPrefix(
    const blink::StorageKey& storage_key) {
  const char kDataPrefix = '_';
  const std::string serialized_storage_key =
      storage_key.SerializeForLocalStorage();
  const char kStorageKeySeparator = '\x00';

  DomStorageDatabase::Key prefix;
  prefix.reserve(serialized_storage_key.size() + 2);
  prefix.push_back(kDataPrefix);
  prefix.insert(prefix.end(), serialized_storage_key.begin(),
                serialized_storage_key.end());
  prefix.push_back(kStorageKeySeparator);
  return prefix;
}

void DeleteStorageKeys(AsyncDomStorageDatabase* database,
                       std::vector<blink::StorageKey> storage_keys,
                       base::OnceCallback<void(leveldb::Status)> callback) {
  database->RunDatabaseTask(
      base::BindOnce(
          [](std::vector<blink::StorageKey> storage_keys,
             const DomStorageDatabase& db) {
            leveldb::WriteBatch batch;
            for (const auto& storage_key : storage_keys) {
              db.DeletePrefixed(MakeStorageKeyPrefix(storage_key), &batch);
              batch.Delete(
                  leveldb_env::MakeSlice(CreateMetaDataKey(storage_key)));
            }
            return db.Commit(&batch);
          },
          storage_keys),
      std::move(callback));
}
StorageAreaImpl::Options createOptions() {
  // Delay for a moment after a value is set in anticipation
  // of other values being set, so changes are batched.
  static constexpr base::TimeDelta kCommitDefaultDelaySecs = base::Seconds(5);

  // To avoid excessive IO we apply limits to the amount of data being written
  // and the frequency of writes.
  static const size_t kMaxBytesPerHour = kPerStorageAreaQuota;
  static constexpr int kMaxCommitsPerHour = 60;

  StorageAreaImpl::Options options;
  options.max_size = kPerStorageAreaQuota + kPerStorageAreaOverQuotaAllowance;
  options.default_commit_delay = kCommitDefaultDelaySecs;
  options.max_bytes_per_hour = kMaxBytesPerHour;
  options.max_commits_per_hour = kMaxCommitsPerHour;
#if BUILDFLAG(IS_ANDROID)
    options.cache_mode = StorageAreaImpl::CacheMode::KEYS_ONLY_WHEN_POSSIBLE;
#else
    options.cache_mode = StorageAreaImpl::CacheMode::KEYS_AND_VALUES;
    if (base::SysInfo::IsLowEndDevice()) {
      options.cache_mode = StorageAreaImpl::CacheMode::KEYS_ONLY_WHEN_POSSIBLE;
    }
#endif
    return options;
}
}  // namespace

class LocalStorageImpl::StorageAreaHolder final
    : public StorageAreaImpl::Delegate {
 public:
  StorageAreaHolder(LocalStorageImpl* context,
                    const blink::StorageKey& storage_key)
      : context_(context),
        storage_key_(storage_key),
        area_(context_->database_.get(),
              MakeStorageKeyPrefix(storage_key_),
              this,
              createOptions()) {}

  StorageAreaImpl* storage_area() { return &area_; }

  void OnNoBindings() override {
    has_bindings_ = false;
    // Don't delete ourselves, but do schedule an immediate commit. Possible
    // deletion will happen under memory pressure or when another localstorage
    // area is opened.
    storage_area()->ScheduleImmediateCommit();
  }

  void PrepareToCommit(
      std::vector<DomStorageDatabase::KeyValuePair>* extra_entries_to_add,
      std::vector<DomStorageDatabase::Key>* extra_keys_to_delete) override {
    // Write schema version if not already done so before.
    if (!context_->database_initialized_) {
      const std::string version =
          base::NumberToString(kCurrentLocalStorageSchemaVersion);
      extra_entries_to_add->emplace_back(
          DomStorageDatabase::Key(kVersionKey.begin(), kVersionKey.end()),
          DomStorageDatabase::Value(version.begin(), version.end()));
      context_->database_initialized_ = true;
    }

    DomStorageDatabase::Key metadata_key = CreateMetaDataKey(storage_key_);
    if (storage_area()->empty()) {
      extra_keys_to_delete->push_back(std::move(metadata_key));
    } else {
      storage::LocalStorageStorageKeyMetaData data;
      data.set_last_modified(base::Time::Now().ToInternalValue());
      data.set_size_bytes(storage_area()->storage_used());
      std::string serialized_data = data.SerializeAsString();
      extra_entries_to_add->emplace_back(
          std::move(metadata_key),
          DomStorageDatabase::Value(serialized_data.begin(),
                                    serialized_data.end()));
    }
  }

  void DidCommit(leveldb::Status status) override {
    context_->OnCommitResult(status);
  }
  void Bind(mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
    has_bindings_ = true;
    storage_area()->Bind(std::move(receiver));
  }

  bool has_bindings() const { return has_bindings_; }

 private:
  raw_ptr<LocalStorageImpl> context_;
  blink::StorageKey storage_key_;
  StorageAreaImpl area_;
  bool has_bindings_ = false;
};

LocalStorageImpl::LocalStorageImpl(
    const base::FilePath& storage_root,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingReceiver<mojom::LocalStorageControl> receiver)
    : directory_(storage_root.empty() ? storage_root
                                      : storage_root.Append(kLocalStoragePath)),
      leveldb_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      memory_dump_id_(base::StringPrintf("LocalStorage/0x%" PRIXPTR,
                                         reinterpret_cast<uintptr_t>(this))),
      is_low_end_device_(base::SysInfo::IsLowEndDevice()) {
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "LocalStorage", task_runner, MemoryDumpProvider::Options());

  if (receiver)
    control_receiver_.Bind(std::move(receiver));
}

void LocalStorageImpl::BindStorageArea(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&LocalStorageImpl::BindStorageArea,
                                    weak_ptr_factory_.GetWeakPtr(), storage_key,
                                    std::move(receiver)));
    return;
  }

  GetOrCreateStorageArea(storage_key)->Bind(std::move(receiver));
}

void LocalStorageImpl::GetUsage(GetUsageCallback callback) {
  RunWhenConnected(base::BindOnce(&LocalStorageImpl::RetrieveStorageUsage,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(callback)));
}

void LocalStorageImpl::DeleteStorage(const blink::StorageKey& storage_key,
                                     DeleteStorageCallback callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&LocalStorageImpl::DeleteStorage,
                                    weak_ptr_factory_.GetWeakPtr(), storage_key,
                                    std::move(callback)));
    return;
  }

  auto found = areas_.find(storage_key);
  if (found != areas_.end()) {
    // Renderer process expects |source| to always be two newline separated
    // strings. We don't bother passing an observer because this is a one-shot
    // event and we only care about observing its completion, for which the
    // reply alone is sufficient.
    found->second->storage_area()->DeleteAll(
        "\n", /*new_observer=*/mojo::NullRemote(),
        base::BindOnce(&SuccessResponse, std::move(callback)));
    found->second->storage_area()->ScheduleImmediateCommit();
  } else if (database_) {
    DeleteStorageKeys(
        database_.get(), {storage_key},
        base::BindOnce([](base::OnceClosure callback,
                          leveldb::Status) { std::move(callback).Run(); },
                       std::move(callback)));
  } else {
    std::move(callback).Run();
  }
}

void LocalStorageImpl::CleanUpStorage(CleanUpStorageCallback callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&LocalStorageImpl::CleanUpStorage,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback)));
    return;
  }

  if (database_) {
    // Try to commit all changes before rewriting the database. If
    // an area is not ready to commit its changes, nothing breaks but the
    // rewrite doesn't remove all traces of old data.
    Flush(base::DoNothing());
    database_->RewriteDB(base::BindOnce(&IgnoreStatus, std::move(callback)));
  } else {
    std::move(callback).Run();
  }
}

void LocalStorageImpl::Flush(FlushCallback callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&LocalStorageImpl::Flush,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback)));
    return;
  }

  base::RepeatingClosure commit_callback = base::BarrierClosure(
      base::saturated_cast<int>(areas_.size()), std::move(callback));
  for (const auto& it : areas_)
    it.second->storage_area()->ScheduleImmediateCommit(commit_callback);
}

void LocalStorageImpl::FlushStorageKeyForTesting(
    const blink::StorageKey& storage_key) {
  if (connection_state_ != CONNECTION_FINISHED)
    return;
  const auto& it = areas_.find(storage_key);
  if (it == areas_.end())
    return;
  it->second->storage_area()->ScheduleImmediateCommit();
}

void LocalStorageImpl::ShutDown(base::OnceClosure callback) {
  DCHECK_NE(connection_state_, CONNECTION_SHUTDOWN);
  DCHECK(callback);

  control_receiver_.reset();
  shutdown_complete_callback_ = std::move(callback);

  // Nothing to do if no connection to the database was ever finished.
  if (connection_state_ != CONNECTION_FINISHED) {
    connection_state_ = CONNECTION_SHUTDOWN;
    OnShutdownComplete();
    return;
  }

  connection_state_ = CONNECTION_SHUTDOWN;

  // Flush any uncommitted data.
  for (const auto& it : areas_) {
    auto* area = it.second->storage_area();
    LOCAL_HISTOGRAM_BOOLEAN("LocalStorageContext.ShutDown.MaybeDroppedChanges",
                            area->has_pending_load_tasks());
    area->ScheduleImmediateCommit();
    // TODO(dmurph): Monitor the above histogram, and if dropping changes is
    // common then handle that here.
    area->CancelAllPendingRequests();
  }

  // Respect the content policy settings about what to
  // keep and what to discard.
  if (force_keep_session_state_) {
    OnShutdownComplete();
    return;  // Keep everything.
  }

  if (!storage_keys_to_purge_on_shutdown_.empty()) {
    RetrieveStorageUsage(
        base::BindOnce(&LocalStorageImpl::OnGotStorageUsageForShutdown,
                       base::Unretained(this)));
  } else {
    OnShutdownComplete();
  }
}

void LocalStorageImpl::PurgeMemory() {
  for (auto it = areas_.begin(); it != areas_.end();) {
    if (it->second->has_bindings()) {
      it->second->storage_area()->PurgeMemory();
      ++it;
    } else {
      it = areas_.erase(it);
    }
  }
}

void LocalStorageImpl::ApplyPolicyUpdates(
    std::vector<mojom::StoragePolicyUpdatePtr> policy_updates) {
  for (const auto& update : policy_updates) {
    // TODO(https://crbug.com/1199077): Pass the real StorageKey when
    // StoragePolicyUpdate is converted.
    blink::StorageKey storage_key(update->origin);
    if (!update->purge_on_shutdown)
      storage_keys_to_purge_on_shutdown_.erase(storage_key);
    else
      storage_keys_to_purge_on_shutdown_.insert(std::move(storage_key));
  }
}

void LocalStorageImpl::PurgeUnusedAreasIfNeeded() {
  size_t total_cache_size, unused_area_count;
  GetStatistics(&total_cache_size, &unused_area_count);

  // Nothing to purge.
  if (!unused_area_count)
    return;

  // No purge is needed.
  if (total_cache_size <= kMaxLocalStorageCacheSize &&
      areas_.size() <= kMaxLocalStorageAreaCount && !is_low_end_device_) {
    return;
  }

  for (auto it = areas_.begin(); it != areas_.end();) {
    if (it->second->has_bindings())
      ++it;
    else
      it = areas_.erase(it);
  }
}

void LocalStorageImpl::ForceKeepSessionState() {
  SetForceKeepSessionState();
}

bool LocalStorageImpl::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (connection_state_ != CONNECTION_FINISHED)
    return true;

  std::string context_name =
      base::StringPrintf("site_storage/localstorage/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this));

  // Account for leveldb memory usage, which actually lives in the file service.
  auto* global_dump = pmd->CreateSharedGlobalAllocatorDump(memory_dump_id_);
  // The size of the leveldb dump will be added by the leveldb service.
  auto* leveldb_mad = pmd->CreateAllocatorDump(context_name + "/leveldb");
  // Specifies that the current context is responsible for keeping memory alive.
  int kImportance = 2;
  pmd->AddOwnershipEdge(leveldb_mad->guid(), global_dump->guid(), kImportance);

  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND) {
    size_t total_cache_size, unused_area_count;
    GetStatistics(&total_cache_size, &unused_area_count);
    auto* mad = pmd->CreateAllocatorDump(context_name + "/cache_size");
    mad->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                   base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                   total_cache_size);
    mad->AddScalar("total_areas",
                   base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                   areas_.size());
    return true;
  }
  for (const auto& it : areas_) {
    std::string storage_key_str =
        it.first.GetMemoryDumpString(/*max_length=*/50);
    std::string area_dump_name = base::StringPrintf(
        "%s/%s/0x%" PRIXPTR, context_name.c_str(), storage_key_str.c_str(),
        reinterpret_cast<uintptr_t>(it.second->storage_area()));
    it.second->storage_area()->OnMemoryDump(area_dump_name, pmd);
  }
  return true;
}

void LocalStorageImpl::SetDatabaseOpenCallbackForTesting(
    base::OnceClosure callback) {
  RunWhenConnected(std::move(callback));
}

LocalStorageImpl::~LocalStorageImpl() {
  DCHECK_EQ(connection_state_, CONNECTION_SHUTDOWN);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void LocalStorageImpl::RunWhenConnected(base::OnceClosure callback) {
  DCHECK_NE(connection_state_, CONNECTION_SHUTDOWN);

  // If we don't have a database connection, we'll need to establish one.
  if (connection_state_ == NO_CONNECTION) {
    connection_state_ = CONNECTION_IN_PROGRESS;
    InitiateConnection();
  }

  if (connection_state_ == CONNECTION_IN_PROGRESS) {
    // Queue this OpenLocalStorage call for when we have a level db pointer.
    on_database_opened_callbacks_.push_back(std::move(callback));
    return;
  }

  std::move(callback).Run();
}

void LocalStorageImpl::PurgeAllStorageAreas() {
  for (const auto& it : areas_)
    it.second->storage_area()->CancelAllPendingRequests();
  areas_.clear();
}

void LocalStorageImpl::InitiateConnection(bool in_memory_only) {
  if (connection_state_ == CONNECTION_SHUTDOWN)
    return;

  DCHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);

  if (!directory_.empty() && directory_.IsAbsolute() && !in_memory_only) {
    // We were given a subdirectory to write to, so use a disk-backed database.
    leveldb_env::Options options;
    options.create_if_missing = true;
    options.max_open_files = 0;  // use minimum
    // Default write_buffer_size is 4 MB but that might leave a 3.999
    // memory allocation in RAM from a log file recovery.
    options.write_buffer_size = 64 * 1024;
    options.block_cache = leveldb_chrome::GetSharedWebBlockCache();

    in_memory_ = false;
    database_ = AsyncDomStorageDatabase::OpenDirectory(
        std::move(options), directory_, kLocalStorageLeveldbName,
        memory_dump_id_, leveldb_task_runner_,
        base::BindOnce(&LocalStorageImpl::OnDatabaseOpened,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // We were not given a subdirectory. Use a memory backed database.
  in_memory_ = true;
  database_ = AsyncDomStorageDatabase::OpenInMemory(
      memory_dump_id_, "local-storage", leveldb_task_runner_,
      base::BindOnce(&LocalStorageImpl::OnDatabaseOpened,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LocalStorageImpl::OnDatabaseOpened(leveldb::Status status) {
  if (!status.ok()) {
    // If we failed to open the database, try to delete and recreate the
    // database, or ultimately fallback to an in-memory database.
    DeleteAndRecreateDatabase();
    return;
  }

  // Verify DB schema version.
  if (database_) {
    database_->Get(std::vector<uint8_t>(kVersionKey.begin(), kVersionKey.end()),
                   base::BindOnce(&LocalStorageImpl::OnGotDatabaseVersion,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  OnConnectionFinished();
}

void LocalStorageImpl::OnGotDatabaseVersion(leveldb::Status status,
                                            const std::vector<uint8_t>& value) {
  if (status.IsNotFound()) {
    // New database, nothing more to do. Current version will get written
    // when first data is committed.
  } else if (status.ok()) {
    // Existing database, check if version number matches current schema
    // version.
    int64_t db_version;
    if (!base::StringToInt64(std::string(value.begin(), value.end()),
                             &db_version) ||
        db_version < kMinSchemaVersion ||
        db_version > kCurrentLocalStorageSchemaVersion) {
      DeleteAndRecreateDatabase();
      return;
    }

    database_initialized_ = true;
  } else {
    // Other read error. Possibly database corruption.
    DeleteAndRecreateDatabase();
    return;
  }

  OnConnectionFinished();
}

void LocalStorageImpl::OnConnectionFinished() {
  if (connection_state_ == CONNECTION_SHUTDOWN)
    return;

  DCHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);
  // If connection was opened successfully, reset tried_to_recreate_during_open_
  // to enable recreating the database on future errors.
  if (database_)
    tried_to_recreate_during_open_ = false;

  // |database_| should be known to either be valid or invalid by now. Run our
  // delayed bindings.
  connection_state_ = CONNECTION_FINISHED;
  for (size_t i = 0; i < on_database_opened_callbacks_.size(); ++i)
    std::move(on_database_opened_callbacks_[i]).Run();
  on_database_opened_callbacks_.clear();
}

void LocalStorageImpl::DeleteAndRecreateDatabase() {
  if (connection_state_ == CONNECTION_SHUTDOWN)
    return;

  // We're about to set database_ to null, so delete the StorageAreaImpls
  // that might still be using the old database.
  PurgeAllStorageAreas();

  // Reset state to be in process of connecting. This will cause requests for
  // StorageAreas to be queued until the connection is complete.
  connection_state_ = CONNECTION_IN_PROGRESS;
  commit_error_count_ = 0;
  database_.reset();

  bool recreate_in_memory = false;

  // If tried to recreate database on disk already, try again but this time
  // in memory.
  if (tried_to_recreate_during_open_ && !in_memory_) {
    recreate_in_memory = true;
  } else if (tried_to_recreate_during_open_) {
    // Give up completely, run without any database.
    OnConnectionFinished();
    return;
  }

  tried_to_recreate_during_open_ = true;

  // Destroy database, and try again.
  if (!in_memory_) {
    DomStorageDatabase::Destroy(
        directory_, kLocalStorageLeveldbName, leveldb_task_runner_,
        base::BindOnce(&LocalStorageImpl::OnDBDestroyed,
                       weak_ptr_factory_.GetWeakPtr(), recreate_in_memory));
  } else {
    // No directory, so nothing to destroy. Retrying to recreate will probably
    // fail, but try anyway.
    InitiateConnection(recreate_in_memory);
  }
}

void LocalStorageImpl::OnDBDestroyed(bool recreate_in_memory,
                                     leveldb::Status status) {
  // We're essentially ignoring the status here. Even if destroying failed we
  // still want to go ahead and try to recreate.
  InitiateConnection(recreate_in_memory);
}

LocalStorageImpl::StorageAreaHolder* LocalStorageImpl::GetOrCreateStorageArea(
    const blink::StorageKey& storage_key) {
  DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
  auto found = areas_.find(storage_key);
  if (found != areas_.end()) {
    return found->second.get();
  }

  PurgeUnusedAreasIfNeeded();

  auto holder = std::make_unique<StorageAreaHolder>(this, storage_key);
  StorageAreaHolder* holder_ptr = holder.get();
  areas_[storage_key] = std::move(holder);
  return holder_ptr;
}

void LocalStorageImpl::RetrieveStorageUsage(GetUsageCallback callback) {
  if (!database_) {
    // If for whatever reason no leveldb database is available, no storage is
    // used, so return an array only containing the current areas.
    std::vector<mojom::StorageUsageInfoPtr> result;
    base::Time now = base::Time::Now();
    for (const auto& it : areas_) {
      result.emplace_back(mojom::StorageUsageInfo::New(it.first, 0, now));
    }
    std::move(callback).Run(std::move(result));
  } else {
    database_->RunDatabaseTask(
        base::BindOnce([](const DomStorageDatabase& db) {
          std::vector<DomStorageDatabase::KeyValuePair> data;
          db.GetPrefixed(base::make_span(kMetaPrefix), &data);
          return data;
        }),
        base::BindOnce(&LocalStorageImpl::OnGotMetaData,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void LocalStorageImpl::OnGotMetaData(
    GetUsageCallback callback,
    std::vector<DomStorageDatabase::KeyValuePair> data) {
  std::vector<mojom::StorageUsageInfoPtr> result;
  std::set<blink::StorageKey> storage_keys;
  for (const auto& row : data) {
    absl::optional<blink::StorageKey> storage_key =
        ExtractStorageKeyFromMetaDataKey(row.key);
    if (!storage_key) {
      // TODO(mek): Deal with database corruption.
      continue;
    }
    storage_keys.insert(*storage_key);

    storage::LocalStorageStorageKeyMetaData row_data;
    if (!row_data.ParseFromArray(row.value.data(), row.value.size())) {
      // TODO(mek): Deal with database corruption.
      continue;
    }

    result.emplace_back(mojom::StorageUsageInfo::New(
        storage_key.value(), row_data.size_bytes(),
        base::Time::FromInternalValue(row_data.last_modified())));
  }
  // Add any storage keys for which StorageAreas exist, but which haven't
  // committed any data to disk yet.
  base::Time now = base::Time::Now();
  for (const auto& it : areas_) {
    if (storage_keys.find(it.first) != storage_keys.end())
      continue;
    // Skip any storage keys that definitely don't have any data.
    if (!it.second->storage_area()->has_pending_load_tasks() &&
        it.second->storage_area()->empty()) {
      continue;
    }
    result.emplace_back(mojom::StorageUsageInfo::New(it.first, 0, now));
  }
  std::move(callback).Run(std::move(result));
}

void LocalStorageImpl::OnGotStorageUsageForShutdown(
    std::vector<mojom::StorageUsageInfoPtr> usage) {
  std::vector<blink::StorageKey> storage_keys_to_delete;
  for (const auto& info : usage) {
    if (base::Contains(storage_keys_to_purge_on_shutdown_, info->storage_key))
      storage_keys_to_delete.push_back(info->storage_key);
  }

  if (!storage_keys_to_delete.empty()) {
    DeleteStorageKeys(database_.get(), std::move(storage_keys_to_delete),
                      base::BindOnce(&LocalStorageImpl::OnStorageKeysDeleted,
                                     base::Unretained(this)));
  } else {
    OnShutdownComplete();
  }
}

void LocalStorageImpl::OnStorageKeysDeleted(leveldb::Status status) {
  OnShutdownComplete();
}

void LocalStorageImpl::OnShutdownComplete() {
  DCHECK(shutdown_complete_callback_);
  // Flush any final tasks on the DB task runner before invoking the callback.
  PurgeAllStorageAreas();
  database_.reset();
  leveldb_task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(), std::move(shutdown_complete_callback_));
}

void LocalStorageImpl::GetStatistics(size_t* total_cache_size,
                                     size_t* unused_area_count) {
  *total_cache_size = 0;
  *unused_area_count = 0;
  for (const auto& it : areas_) {
    *total_cache_size += it.second->storage_area()->memory_used();
    if (!it.second->has_bindings())
      (*unused_area_count)++;
  }
}

void LocalStorageImpl::OnCommitResult(leveldb::Status status) {
  DCHECK(connection_state_ == CONNECTION_FINISHED ||
         connection_state_ == CONNECTION_SHUTDOWN)
      << connection_state_;
  if (status.ok()) {
    commit_error_count_ = 0;
    return;
  }

  commit_error_count_++;
  if (commit_error_count_ > kCommitErrorThreshold &&
      connection_state_ != CONNECTION_SHUTDOWN) {
    if (tried_to_recover_from_commit_errors_) {
      // We already tried to recover from a high commit error rate before, but
      // are still having problems: there isn't really anything left to try, so
      // just ignore errors.
      return;
    }
    tried_to_recover_from_commit_errors_ = true;

    // Deleting StorageAreas in here could cause more commits (and commit
    // errors), but those commits won't reach OnCommitResult because the area
    // will have been deleted before the commit finishes.
    DeleteAndRecreateDatabase();
  }
}

}  // namespace storage
