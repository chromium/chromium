// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/local_storage_impl.h"

#include <inttypes.h>

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/barrier_closure.h"
#include "base/byte_size.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/local_storage_database.pb.h"
#include "components/services/storage/dom_storage/leveldb/local_storage_leveldb.h"
#include "components/services/storage/dom_storage/storage_area_impl.h"
#include "components/services/storage/public/cpp/constants.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/common/database/database_identifier.h"
#include "storage/common/database/db_status.h"
#include "storage/common/database/leveldb_status_helper.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace storage {

// For a description of the local storage LevelDB schema, see comments in
// `leveldb/local_storage_leveldb.h`.

namespace {

static const int kStaleBucketCutoffInDays = 400;

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

void SuccessResponse(base::OnceClosure callback, bool success) {
  std::move(callback).Run();
}

void IgnoreStatus(base::OnceClosure callback, DbStatus status) {
  std::move(callback).Run();
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
              LocalStorageLevelDB::GetMapPrefix(storage_key_),
              this,
              createOptions()) {}

  ~StorageAreaHolder() override {
    // If we already wrote last_accessed we can skip writing it again.
    if (has_written_access_meta_data_) {
      return;
    }
    // We should not write last_accessed if the area is empty.
    if (storage_area()->empty()) {
      return;
    }
    // We should not write last_accessed if the data will be purged.
    if (context_->origins_to_purge_on_shutdown_.find(storage_key_.origin()) !=
            context_->origins_to_purge_on_shutdown_.end() ||
        context_->origins_to_purge_on_shutdown_.find(
            url::Origin::Create(storage_key_.top_level_site().GetURL())) !=
            context_->origins_to_purge_on_shutdown_.end()) {
      return;
    }

    // Update the storage area map's last access time.
    DomStorageDatabase::Metadata usage;
    usage.map_metadata.push_back({
        .map_locator{kLocalStorageSessionId, storage_key_},
        .last_accessed{base::Time::Now()},
    });
    context_->database_->PutMetadata(
        std::move(usage), base::BindOnce([](DbStatus status) {
          base::UmaHistogramBoolean(
              "LocalStorage.AccessMetaDataUpdateAtShutdown", status.ok());
        }));
  }

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
    DomStorageDatabase::Key access_metadata_key =
        LocalStorageLevelDB::CreateAccessMetaDataKey(storage_key_);
    DomStorageDatabase::Key write_metadata_key =
        LocalStorageLevelDB::CreateWriteMetaDataKey(storage_key_);
    if (storage_area()->empty()) {
      extra_keys_to_delete->push_back(std::move(access_metadata_key));
      extra_keys_to_delete->push_back(std::move(write_metadata_key));
    } else {
      base::Time now = base::Time::Now();
      base::ByteSize total_size{storage_area()->storage_used()};
      extra_entries_to_add->emplace_back(
          std::move(write_metadata_key),
          LocalStorageLevelDB::CreateWriteMetaDataValue(/*last_modified=*/now,
                                                        total_size));
      // We only need to write this once per construction.
      if (!has_written_access_meta_data_) {
        storage::LocalStorageAreaAccessMetaData access_data;
        access_data.set_last_accessed(now.ToInternalValue());
        std::string serialized_access_data = access_data.SerializeAsString();
        extra_entries_to_add->emplace_back(
            std::move(access_metadata_key),
            DomStorageDatabase::Value(serialized_access_data.begin(),
                                      serialized_access_data.end()));
        has_written_access_meta_data_ = true;
      }
    }
  }

  void DidCommit(DbStatus status) override { context_->OnCommitResult(status); }
  void Bind(mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
    has_bindings_ = true;
    storage_area()->Bind(std::move(receiver));
  }

  bool has_bindings() const { return has_bindings_; }

 private:
  raw_ptr<LocalStorageImpl> context_;
  blink::StorageKey storage_key_;
  bool has_bindings_ = false;
  bool has_written_access_meta_data_ = false;
  StorageAreaImpl area_;
};

LocalStorageImpl::LocalStorageImpl(
    const base::FilePath& storage_root,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    DestructLocalStorageCallback destruct_callback,
    mojo::PendingReceiver<mojom::LocalStorageControl> receiver)
    : destruct_callback_(std::move(destruct_callback)),
      directory_(storage_root.empty() ? storage_root
                                      : storage_root.Append(kLocalStoragePath)),
      database_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      memory_dump_id_(base::StringPrintf("LocalStorage/0x%" PRIXPTR,
                                         reinterpret_cast<uintptr_t>(this))) {
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "LocalStorage", task_runner, MemoryDumpProvider::Options());

  if (receiver) {
    control_receiver_.Bind(std::move(receiver));
    control_receiver_.set_disconnect_handler(
        base::BindOnce(&LocalStorageImpl::OnReceiverDisconnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }
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
    database_->DeleteStorageKeysFromSession(
        kLocalStorageSessionId, {storage_key}, /*excluded_cloned_map_ids=*/{},
        base::BindOnce([](base::OnceClosure callback,
                          DbStatus) { std::move(callback).Run(); },
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
    Flush();
    database_->RewriteDB(base::BindOnce(&IgnoreStatus, std::move(callback)));
  } else {
    std::move(callback).Run();
  }
}

void LocalStorageImpl::Flush() {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&LocalStorageImpl::Flush,
                                    weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  for (const auto& it : areas_)
    it.second->storage_area()->ScheduleImmediateCommit();
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
  CHECK_NE(connection_state_, CONNECTION_SHUTDOWN);
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

  if (!origins_to_purge_on_shutdown_.empty()) {
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
    const url::Origin origin = update->origin;
    if (!update->purge_on_shutdown)
      origins_to_purge_on_shutdown_.erase(origin);
    else
      origins_to_purge_on_shutdown_.insert(std::move(origin));
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
      areas_.size() <= kMaxLocalStorageAreaCount &&
      !base::SysInfo::IsLowEndDevice()) {
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
      base::trace_event::MemoryDumpLevelOfDetail::kBackground) {
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

base::FilePath LocalStorageImpl::GetStoragePath() const {
  if (directory_.empty()) {
    return directory_;
  }
  return directory_.DirName();
}

void LocalStorageImpl::SetDatabaseOpenCallbackForTesting(
    base::OnceClosure callback) {
  RunWhenConnected(std::move(callback));
}

void LocalStorageImpl::OverrideDeleteStaleStorageAreasDelayForTesting(
    const base::TimeDelta& delay) {
  delete_stale_storage_areas_delay_ = delay;
}

void LocalStorageImpl::ForceFakeOpenStorageAreaForTesting(
    const blink::StorageKey& storage_key) {
  areas_[storage_key] = std::make_unique<StorageAreaHolder>(this, storage_key);
}

LocalStorageImpl::~LocalStorageImpl() {
  DCHECK_EQ(connection_state_, CONNECTION_SHUTDOWN);
  // ShutDown() should run before this destructor and clear `areas_`. If this
  // didn't occur, as a workaround, we clear the `areas_`to avoid a UaF crash
  // in the StorageAreaHolder d'tor which tries to access `this`'s state.
  // TODO(crbug.com/396030877): Remove this workaround once the issue is
  // resolved.
  if (!areas_.empty()) {
    areas_.clear();
  }
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
    in_memory_ = false;
    database_ = AsyncDomStorageDatabase::Open(
        StorageType::kLocalStorage, directory_, kLocalStorageLeveldbName,
        memory_dump_id_, database_task_runner_,
        base::BindOnce(&LocalStorageImpl::OnDatabaseOpened,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // We were not given a subdirectory. Use a memory backed database.
  in_memory_ = true;
  database_ = AsyncDomStorageDatabase::Open(
      StorageType::kLocalStorage,
      /*directory=*/base::FilePath(), "local-storage", memory_dump_id_,
      database_task_runner_,
      base::BindOnce(&LocalStorageImpl::OnDatabaseOpened,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LocalStorageImpl::OnDatabaseOpened(DbStatus status) {
  LogLevelDBStatusHistogram("LocalStorage.DatabaseOpen", status);

  if (!status.ok()) {
    // If we failed to open the database, try to delete and recreate the
    // database, or ultimately fallback to an in-memory database.
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

  // Clear stale storage areas after a delay to prevent blocking session
  // restoration.
  if (database_ && !in_memory_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&LocalStorageImpl::DeleteStaleStorageAreas,
                       weak_ptr_factory_.GetWeakPtr()),
        delete_stale_storage_areas_delay_);
  }

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
    DomStorageDatabaseFactory::Destroy(
        directory_, kLocalStorageLeveldbName, database_task_runner_,
        base::BindOnce(&LocalStorageImpl::OnDBDestroyed,
                       weak_ptr_factory_.GetWeakPtr(), recreate_in_memory));
  } else {
    // No directory, so nothing to destroy. Retrying to recreate will probably
    // fail, but try anyway.
    InitiateConnection(recreate_in_memory);
  }
}

void LocalStorageImpl::OnDBDestroyed(bool recreate_in_memory, DbStatus status) {
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
    database_->ReadAllMetadata(
        base::BindOnce(&LocalStorageImpl::OnGotWriteMetaData,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void LocalStorageImpl::OnGotWriteMetaData(
    GetUsageCallback callback,
    StatusOr<DomStorageDatabase::Metadata> all_metadata) {
  std::vector<mojom::StorageUsageInfoPtr> result;
  std::set<blink::StorageKey> storage_keys;

  // Update `result` to include maps that have committed data to disk.
  if (all_metadata.has_value()) {
    for (const DomStorageDatabase::MapMetadata& usage_metadata :
         all_metadata->map_metadata) {
      if (usage_metadata.last_modified && usage_metadata.total_size) {
        const blink::StorageKey& storage_key =
            usage_metadata.map_locator.storage_key();
        storage_keys.insert(storage_key);

        result.emplace_back(mojom::StorageUsageInfo::New(
            storage_key, usage_metadata.total_size->InBytes(),
            *usage_metadata.last_modified));
      }
    }
  }

  // Add any storage keys for which StorageAreas exist, but which haven't
  // committed any data to disk yet.
  base::Time now = base::Time::Now();
  for (const auto& it : areas_) {
    if (storage_keys.find(it.first) != storage_keys.end())
      continue;
    StorageAreaImpl* storage_area = it.second->storage_area();
    // Skip any storage keys that definitely don't have any data.
    if (!storage_area->has_pending_load_tasks() && storage_area->empty()) {
      continue;
    }
    result.emplace_back(mojom::StorageUsageInfo::New(
        it.first, storage_area->storage_used(), now));
  }
  std::move(callback).Run(std::move(result));
}

void LocalStorageImpl::OnGotStorageUsageForShutdown(
    std::vector<mojom::StorageUsageInfoPtr> usage) {
  std::vector<blink::StorageKey> storage_keys_to_delete;
  for (const auto& info : usage) {
    const blink::StorageKey& storage_key = info->storage_key;
    const url::Origin& key_origin = storage_key.origin();
    // Ideally we would be recording last_accessed instead, but there is no
    // historical data on that. Instead, we will use last_modified as a sanity
    // check against other data as we try to understand how many 'old' storage
    // buckets are still in use. This is split into two buckets for greater
    // resolution on near and far term ages.
    if (!info->last_modified.is_null() &&
        info->last_modified < base::Time::Now()) {
      int days_since_last_modified =
          (base::Time::Now() - info->last_modified).InDays();
      base::UmaHistogramCustomCounts("LocalStorage.DaysSinceLastModified",
                                     days_since_last_modified, 1,
                                     kStaleBucketCutoffInDays, 100);
    }
    // Delete the storage if its origin matches one of the origins to purge, or
    // if it is third-party and the top-level site is same-site with one of
    // those origins.
    for (const auto& origin_to_purge : origins_to_purge_on_shutdown_) {
      if (key_origin == origin_to_purge ||
          (storage_key.IsThirdPartyContext() &&
           storage_key.top_level_site().IsSameSiteWith(origin_to_purge))) {
        storage_keys_to_delete.push_back(storage_key);
        break;
      }
    }
  }

  if (!storage_keys_to_delete.empty() && database_) {
    database_->DeleteStorageKeysFromSession(
        kLocalStorageSessionId, std::move(storage_keys_to_delete),
        /*excluded_cloned_map_ids=*/{},
        base::BindOnce(&LocalStorageImpl::OnStorageKeysDeleted,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnShutdownComplete();
  }
}

void LocalStorageImpl::OnStorageKeysDeleted(DbStatus status) {
  OnShutdownComplete();
}

void LocalStorageImpl::OnShutdownComplete() {
  DCHECK(shutdown_complete_callback_);
  // Flush any final tasks on the DB task runner before invoking the callback.
  PurgeAllStorageAreas();
  database_.reset();
  database_task_runner_->PostTaskAndReply(
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

void LocalStorageImpl::OnCommitResult(DbStatus status) {
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

void LocalStorageImpl::DeleteStaleStorageAreas() {
  if (!database_) {
    // Due to the delay before LocalStorageImpl::DeleteStaleStorageAreas is invoked
    // it's possible `database_` existed before, but no longer.
    return;
  }
  database_->ReadAllMetadata(
      base::BindOnce(&LocalStorageImpl::OnGotMetaDataToDeleteStaleStorageAreas,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LocalStorageImpl::OnGotMetaDataToDeleteStaleStorageAreas(
    StatusOr<DomStorageDatabase::Metadata> all_metadata) {
  if (!database_) {
    // This method is provided as a callback to an off thread task. Between the
    // time that the task is posted and now when this callback is invoked, the
    // `database_` member may have been reset.
    return;
  }
  if (!all_metadata.has_value()) {
    // ERROR: Failed to read from the database!
    return;
  }
  // Filter and collect stale storage areas for deletion.
  std::vector<blink::StorageKey> stale_storage_keys;
  uint64_t orphans_found = 0;
  for (const DomStorageDatabase::MapMetadata& usage_metadata :
       all_metadata->map_metadata) {
    const blink::StorageKey& storage_key =
        usage_metadata.map_locator.storage_key();
    if (areas_.find(storage_key) != areas_.end()) {
      // If the storage area is currently loaded it must not be cleared.
      continue;
    }

    // Use the most recent last accessed time or last modified time.
    base::Time accessed_or_modified_time;
    if (usage_metadata.last_accessed && usage_metadata.last_modified) {
      accessed_or_modified_time = std::max(*usage_metadata.last_accessed,
                                           *usage_metadata.last_modified);
    } else if (usage_metadata.last_modified) {
      accessed_or_modified_time = *usage_metadata.last_modified;
    } else {
      accessed_or_modified_time = usage_metadata.last_accessed.value();
    }

    if ((base::Time::Now() - accessed_or_modified_time) >=
        base::Days(kStaleBucketCutoffInDays)) {
      // If the storage area has not been accessed or modified within 400 days
      // it can be cleared.
      stale_storage_keys.push_back(storage_key);
    } else if ((storage_key.nonce().has_value() ||
                storage_key.top_level_site().opaque()) &&
               (base::Time::Now() - accessed_or_modified_time) >=
                   base::Days(1)) {
      // If the storage area has not been accessed or modified in this browsing
      // session and is transient (has a nonce) then it can be cleared.
      stale_storage_keys.push_back(storage_key);
      orphans_found++;
    }
  }
  // These are counted independently to better track errors in rollout.
  base::UmaHistogramCounts100000(
      "LocalStorage.OrphanStorageAreasOnStartupCount", orphans_found);

  // Delete stale storage areas and count results.
  size_t deleted_count = stale_storage_keys.size();
  database_->DeleteStorageKeysFromSession(
      kLocalStorageSessionId, std::move(stale_storage_keys),
      /*excluded_cloned_map_ids=*/{},
      base::BindOnce(
          [](size_t keys_deleted, DbStatus status) {
            base::UmaHistogramBoolean(
                "LocalStorage.StaleStorageAreasDeletedOnStartupSuccess",
                status.ok());
            if (status.ok()) {
              base::UmaHistogramCounts100000(
                  "LocalStorage.StaleStorageAreasDeletedOnStartupCount",
                  keys_deleted);
            }
          },
          deleted_count));
}

void LocalStorageImpl::OnReceiverDisconnected() {
  std::move(destruct_callback_).Run(this);
}

}  // namespace storage
