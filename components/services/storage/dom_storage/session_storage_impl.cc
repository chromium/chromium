// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_impl.h"

#include <inttypes.h>

#include <utility>

#include "base/barrier_closure.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/session_storage_area_impl.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace {
// After this many consecutive commit errors we'll throw away the entire
// database.
const int kSessionStorageCommitErrorThreshold = 8;

// Limits on the cache size and number of areas in memory, over which the areas
// are purged.
#if BUILDFLAG(IS_ANDROID)
const unsigned kMaxSessionStorageAreaCount = 10;
const size_t kMaxSessionStorageCacheSize = 2 * 1024 * 1024;
#else
const unsigned kMaxSessionStorageAreaCount = 50;
const size_t kMaxSessionStorageCacheSize = 20 * 1024 * 1024;
#endif

enum class SessionStorageCachePurgeReason {
  kNotNeeded,
  kSizeLimitExceeded,
  kAreaCountLimitExceeded,
  kInactiveOnLowEndDevice,
  kAggressivePurgeTriggered
};

void RecordSessionStorageCachePurgedHistogram(
    SessionStorageCachePurgeReason reason,
    size_t purged_size_kib) {
  UMA_HISTOGRAM_COUNTS_100000("SessionStorageContext.CachePurgedInKB",
                              purged_size_kib);
  switch (reason) {
    case SessionStorageCachePurgeReason::kSizeLimitExceeded:
      UMA_HISTOGRAM_COUNTS_100000(
          "SessionStorageContext.CachePurgedInKB.SizeLimitExceeded",
          purged_size_kib);
      break;
    case SessionStorageCachePurgeReason::kAreaCountLimitExceeded:
      UMA_HISTOGRAM_COUNTS_100000(
          "SessionStorageContext.CachePurgedInKB.AreaCountLimitExceeded",
          purged_size_kib);
      break;
    case SessionStorageCachePurgeReason::kInactiveOnLowEndDevice:
      UMA_HISTOGRAM_COUNTS_100000(
          "SessionStorageContext.CachePurgedInKB.InactiveOnLowEndDevice",
          purged_size_kib);
      break;
    case SessionStorageCachePurgeReason::kAggressivePurgeTriggered:
      UMA_HISTOGRAM_COUNTS_100000(
          "SessionStorageContext.CachePurgedInKB.AggressivePurgeTriggered",
          purged_size_kib);
      break;
    case SessionStorageCachePurgeReason::kNotNeeded:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void SessionStorageErrorResponse(base::OnceClosure callback,
                                 leveldb::Status status) {
  std::move(callback).Run();
}

}  // namespace

SessionStorageImpl::SessionStorageImpl(
    const base::FilePath& partition_directory,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    scoped_refptr<base::SequencedTaskRunner> memory_dump_task_runner,
    BackingMode backing_mode,
    std::string database_name,
    mojo::PendingReceiver<mojom::SessionStorageControl> receiver)
    : backing_mode_(backing_mode),
      database_name_(std::move(database_name)),
      partition_directory_(partition_directory),
      database_task_runner_(std::move(blocking_task_runner)),
      memory_dump_id_(base::StringPrintf("SessionStorage/0x%" PRIXPTR,
                                         reinterpret_cast<uintptr_t>(this))),
      receiver_(this, std::move(receiver)),
      is_low_end_mode_(
          base::SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled()) {
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "SessionStorage", std::move(memory_dump_task_runner),
          base::trace_event::MemoryDumpProvider::Options());
}

SessionStorageImpl::~SessionStorageImpl() {
  DCHECK_EQ(connection_state_, CONNECTION_SHUTDOWN);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void SessionStorageImpl::BindNamespace(
    const std::string& namespace_id,
    mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver,
    BindNamespaceCallback callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(
        &SessionStorageImpl::BindNamespace, weak_ptr_factory_.GetWeakPtr(),
        namespace_id, std::move(receiver), std::move(callback)));
    return;
  }
  auto found = namespaces_.find(namespace_id);
  if (found == namespaces_.end()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (found->second->state() ==
      SessionStorageNamespaceImpl::State::kNotPopulated) {
    found->second->PopulateFromMetadata(
        database_.get(), metadata_.GetOrCreateNamespaceEntry(namespace_id));
  }

  PurgeUnusedAreasIfNeeded();
  found->second->Bind(std::move(receiver));

  size_t total_cache_size, unused_area_count;
  GetStatistics(&total_cache_size, &unused_area_count);
  // Track the total sessionStorage cache size.
  UMA_HISTOGRAM_COUNTS_100000("SessionStorageContext.CacheSizeInKB",
                              total_cache_size / 1024);
  std::move(callback).Run(/*success=*/true);
}

void SessionStorageImpl::BindStorageArea(
    const blink::StorageKey& storage_key,
    const std::string& namespace_id,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver,
    BindStorageAreaCallback callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(
        &SessionStorageImpl::BindStorageArea, weak_ptr_factory_.GetWeakPtr(),
        storage_key, namespace_id, std::move(receiver), std::move(callback)));
    return;
  }

  auto found = namespaces_.find(namespace_id);
  if (found == namespaces_.end()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (found->second->state() ==
      SessionStorageNamespaceImpl::State::kNotPopulated) {
    found->second->PopulateFromMetadata(
        database_.get(), metadata_.GetOrCreateNamespaceEntry(namespace_id));
  }

  PurgeUnusedAreasIfNeeded();
  found->second->OpenArea(storage_key, std::move(receiver));
  std::move(callback).Run(/*success=*/true);
}

void SessionStorageImpl::CreateNamespace(const std::string& namespace_id) {
  DCHECK_NE(connection_state_, CONNECTION_IN_PROGRESS);
  if (namespaces_.find(namespace_id) != namespaces_.end())
    return;

  namespaces_.emplace(std::make_pair(
      namespace_id, CreateSessionStorageNamespaceImpl(namespace_id)));
}

void SessionStorageImpl::CloneNamespace(
    const std::string& clone_from_namespace_id,
    const std::string& clone_to_namespace_id,
    mojom::SessionStorageCloneType clone_type) {
  DCHECK_NE(connection_state_, CONNECTION_IN_PROGRESS);
  if (namespaces_.find(clone_to_namespace_id) != namespaces_.end()) {
    // Non-immediate clones expect to be paired with a |Clone| from the mojo
    // namespace object. If that clone has already happened, then we don't need
    // to do anything here.
    // However, immediate clones happen without a |Clone| from the mojo
    // namespace object, so there should never be a namespace already populated
    // for an immediate clone.
    DCHECK_NE(clone_type, mojom::SessionStorageCloneType::kImmediate);
    return;
  }

  auto clone_from_ns = namespaces_.find(clone_from_namespace_id);
  std::unique_ptr<SessionStorageNamespaceImpl> clone_to_namespace_impl =
      CreateSessionStorageNamespaceImpl(clone_to_namespace_id);
  switch (clone_type) {
    case mojom::SessionStorageCloneType::kImmediate: {
      // If the namespace doesn't exist or it's not populated yet, just create
      // an empty session storage by not marking it as pending a clone.
      if (clone_from_ns == namespaces_.end() ||
          !clone_from_ns->second->IsPopulated()) {
        break;
      }
      clone_from_ns->second->Clone(clone_to_namespace_id);
      return;
    }
    case mojom::SessionStorageCloneType::kWaitForCloneOnNamespace:
      if (clone_from_ns != namespaces_.end()) {
        // The namespace exists and is in-use, so wait until receiving a clone
        // call on that mojo binding.
        clone_to_namespace_impl->SetPendingPopulationFromParentNamespace(
            clone_from_namespace_id);
        clone_from_ns->second->AddChildNamespaceWaitingForClone(
            clone_to_namespace_id);
      } else if (base::Contains(metadata_.namespace_storage_key_map(),
                                clone_from_namespace_id)) {
        DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
        // The namespace exists on disk but is not in-use, so do the appropriate
        // metadata operations to clone the namespace and set up the new object.
        std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
        auto source_namespace_entry =
            metadata_.GetOrCreateNamespaceEntry(clone_from_namespace_id);
        auto namespace_entry =
            metadata_.GetOrCreateNamespaceEntry(clone_to_namespace_id);
        metadata_.RegisterShallowClonedNamespace(source_namespace_entry,
                                                 namespace_entry, &save_tasks);
        if (database_) {
          database_->RunBatchDatabaseTasks(
              std::move(save_tasks),
              base::BindOnce(&SessionStorageImpl::OnCommitResult,
                             weak_ptr_factory_.GetWeakPtr()));
        }
      }
      // If there is no sign of a source namespace, just run with an empty
      // namespace.
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  namespaces_.emplace(
      std::piecewise_construct, std::forward_as_tuple(clone_to_namespace_id),
      std::forward_as_tuple(std::move(clone_to_namespace_impl)));
}

void SessionStorageImpl::DeleteNamespace(const std::string& namespace_id,
                                         bool should_persist) {
  DCHECK_NE(connection_state_, CONNECTION_IN_PROGRESS);
  auto namespace_it = namespaces_.find(namespace_id);
  // If the namespace has pending clones, do the clone now before destroying it.
  if (namespace_it != namespaces_.end()) {
    SessionStorageNamespaceImpl* namespace_ptr = namespace_it->second.get();
    if (namespace_ptr->HasChildNamespacesWaitingForClone()) {
      // Wait until we are connected, as it simplifies our choices.
      if (connection_state_ != CONNECTION_FINISHED) {
        RunWhenConnected(base::BindOnce(&SessionStorageImpl::DeleteNamespace,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        namespace_id, should_persist));
        return;
      }
      namespace_ptr->CloneAllNamespacesWaitingForClone(database_.get(),
                                                       &metadata_, namespaces_);
    }

    // The object hierarchy uses iterators bound to the metadata object, so
    // make sure to delete the object hierarchy first.
    namespaces_.erase(namespace_it);
  }

  if (!has_scavenged_ && should_persist)
    protected_namespaces_from_scavenge_.insert(namespace_id);

  if (!should_persist) {
    RunWhenConnected(base::BindOnce(&SessionStorageImpl::DoDatabaseDelete,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    namespace_id));
  }
}

void SessionStorageImpl::Flush() {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&SessionStorageImpl::Flush,
                                    weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  for (const auto& it : data_maps_)
    it.second->storage_area()->ScheduleImmediateCommit();
}

void SessionStorageImpl::GetUsage(GetUsageCallback callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&SessionStorageImpl::GetUsage,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback)));
    return;
  }

  const SessionStorageMetadata::NamespaceStorageKeyMap& all_namespaces =
      metadata_.namespace_storage_key_map();

  std::vector<mojom::SessionStorageUsageInfoPtr> result;
  result.reserve(all_namespaces.size());
  for (const auto& pair : all_namespaces) {
    for (const auto& storage_key_map_pair : pair.second) {
      result.push_back(mojom::SessionStorageUsageInfo::New(
          storage_key_map_pair.first, pair.first));
    }
  }
  std::move(callback).Run(std::move(result));
}

void SessionStorageImpl::DeleteStorage(const blink::StorageKey& storage_key,
                                       const std::string& namespace_id,
                                       DeleteStorageCallback callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&SessionStorageImpl::DeleteStorage,
                                    weak_ptr_factory_.GetWeakPtr(), storage_key,
                                    namespace_id, std::move(callback)));
    return;
  }
  auto found = namespaces_.find(namespace_id);
  if (found != namespaces_.end() &&
      found->second->state() !=
          SessionStorageNamespaceImpl::State::kNotPopulated) {
    found->second->RemoveStorageKeyData(storage_key, std::move(callback));
  } else {
    // If we don't have the namespace loaded, then we can delete it all
    // using the metadata.
    std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> tasks;
    metadata_.DeleteArea(namespace_id, storage_key, &tasks);
    if (database_) {
      database_->RunBatchDatabaseTasks(
          std::move(tasks),
          base::BindOnce(&SessionStorageImpl::OnCommitResultWithCallback,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    } else {
      std::move(callback).Run();
    }
  }
}

void SessionStorageImpl::CleanUpStorage(CleanUpStorageCallback callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&SessionStorageImpl::CleanUpStorage,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback)));
    return;
  }
  if (database_) {
    for (const auto& it : data_maps_)
      it.second->storage_area()->ScheduleImmediateCommit();
    database_->RewriteDB(
        base::BindOnce(&SessionStorageErrorResponse, std::move(callback)));
  } else {
    std::move(callback).Run();
  }
}

void SessionStorageImpl::ShutDown(base::OnceClosure callback) {
  DCHECK_NE(connection_state_, CONNECTION_SHUTDOWN);
  DCHECK(callback);

  receiver_.reset();
  shutdown_complete_callback_ = std::move(callback);

  // The namespaces will DCHECK if they are destructed with pending clones. It
  // is valid to drop these on shutdown.
  for (auto& namespace_pair : namespaces_) {
    namespace_pair.second->ClearChildNamespacesWaitingForClone();
  }

  // Nothing to do if no connection to the database was ever finished.
  if (connection_state_ != CONNECTION_FINISHED) {
    connection_state_ = CONNECTION_SHUTDOWN;
    OnShutdownComplete();
    return;
  }
  connection_state_ = CONNECTION_SHUTDOWN;

  // Flush any uncommitted data.
  for (const auto& it : data_maps_) {
    auto* area = it.second->storage_area();
    LOCAL_HISTOGRAM_BOOLEAN(
        "SessionStorageContext.ShutDown.MaybeDroppedChanges",
        area->has_pending_load_tasks());
    area->ScheduleImmediateCommit();
    // TODO(dmurph): Monitor the above histogram, and if dropping changes is
    // common then handle that here.
    area->CancelAllPendingRequests();
  }

  OnShutdownComplete();
}

void SessionStorageImpl::PurgeMemory() {
  size_t total_cache_size, unused_area_count;
  GetStatistics(&total_cache_size, &unused_area_count);

  // Purge all areas that don't have bindings.
  for (const auto& namespace_pair : namespaces_) {
    namespace_pair.second->PurgeUnboundAreas();
  }
  // Purge memory from bound maps.
  for (const auto& data_map_pair : data_maps_) {
    data_map_pair.second->storage_area()->PurgeMemory();
  }

  // Track the size of cache purged.
  size_t final_total_cache_size;
  GetStatistics(&final_total_cache_size, &unused_area_count);
  size_t purged_size_kib = (total_cache_size - final_total_cache_size) / 1024;
  RecordSessionStorageCachePurgedHistogram(
      SessionStorageCachePurgeReason::kAggressivePurgeTriggered,
      purged_size_kib);
}

void SessionStorageImpl::PurgeUnusedAreasIfNeeded() {
  size_t total_cache_size, unused_area_count;
  GetStatistics(&total_cache_size, &unused_area_count);

  // Nothing to purge.
  if (!unused_area_count)
    return;

  SessionStorageCachePurgeReason purge_reason =
      SessionStorageCachePurgeReason::kNotNeeded;

  if (total_cache_size > kMaxSessionStorageCacheSize)
    purge_reason = SessionStorageCachePurgeReason::kSizeLimitExceeded;
  else if (data_maps_.size() > kMaxSessionStorageAreaCount)
    purge_reason = SessionStorageCachePurgeReason::kAreaCountLimitExceeded;
  else if (is_low_end_mode_) {
    purge_reason = SessionStorageCachePurgeReason::kInactiveOnLowEndDevice;
  }

  if (purge_reason == SessionStorageCachePurgeReason::kNotNeeded)
    return;

  // Purge all areas that don't have bindings.
  for (const auto& namespace_pair : namespaces_) {
    namespace_pair.second->PurgeUnboundAreas();
  }

  size_t final_total_cache_size;
  GetStatistics(&final_total_cache_size, &unused_area_count);
  size_t purged_size_kib = (total_cache_size - final_total_cache_size) / 1024;
  RecordSessionStorageCachePurgedHistogram(purge_reason, purged_size_kib);
}

void SessionStorageImpl::ScavengeUnusedNamespaces(
    ScavengeUnusedNamespacesCallback callback) {
  if (has_scavenged_) {
    std::move(callback).Run();
    return;
  }
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(
        base::BindOnce(&SessionStorageImpl::ScavengeUnusedNamespaces,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  has_scavenged_ = true;
  std::vector<std::string> namespaces_to_delete;
  for (const auto& metadata_namespace : metadata_.namespace_storage_key_map()) {
    const std::string& namespace_id = metadata_namespace.first;
    if (namespaces_.find(namespace_id) != namespaces_.end() ||
        protected_namespaces_from_scavenge_.find(namespace_id) !=
            protected_namespaces_from_scavenge_.end()) {
      continue;
    }
    namespaces_to_delete.push_back(namespace_id);
  }
  std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
  for (const auto& namespace_id : namespaces_to_delete)
    metadata_.DeleteNamespace(namespace_id, &save_tasks);

  if (database_) {
    database_->RunBatchDatabaseTasks(
        std::move(save_tasks),
        base::BindOnce(&SessionStorageImpl::OnCommitResult,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  protected_namespaces_from_scavenge_.clear();
  std::move(callback).Run();
}

bool SessionStorageImpl::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (connection_state_ != CONNECTION_FINISHED)
    return true;

  std::string context_name =
      base::StringPrintf("site_storage/sessionstorage/0x%" PRIXPTR,
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
                   data_maps_.size());
    return true;
  }
  for (const auto& it : data_maps_) {
    const auto& storage_key = it.second->map_data()->storage_key();
    std::string storage_key_str =
        storage_key.GetMemoryDumpString(/*max_length=*/50);
    std::string area_dump_name = base::StringPrintf(
        "%s/%s/0x%" PRIXPTR, context_name.c_str(), storage_key_str.c_str(),
        reinterpret_cast<uintptr_t>(it.second->storage_area()));
    it.second->storage_area()->OnMemoryDump(area_dump_name, pmd);
  }
  return true;
}

void SessionStorageImpl::PretendToConnectForTesting() {
  OnDatabaseOpened(leveldb::Status::OK());
}

void SessionStorageImpl::FlushAreaForTesting(
    const std::string& namespace_id,
    const blink::StorageKey& storage_key) {
  if (connection_state_ != CONNECTION_FINISHED)
    return;
  const auto& it = namespaces_.find(namespace_id);
  if (it == namespaces_.end())
    return;
  it->second->FlushStorageKeyForTesting(storage_key);
}

void SessionStorageImpl::SetDatabaseOpenCallbackForTesting(
    base::OnceClosure callback) {
  RunWhenConnected(std::move(callback));
}

scoped_refptr<SessionStorageMetadata::MapData>
SessionStorageImpl::RegisterNewAreaMap(
    SessionStorageMetadata::NamespaceEntry namespace_entry,
    const blink::StorageKey& storage_key) {
  std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
  scoped_refptr<SessionStorageMetadata::MapData> map_entry =
      metadata_.RegisterNewMap(namespace_entry, storage_key, &save_tasks);

  if (database_) {
    database_->RunBatchDatabaseTasks(
        std::move(save_tasks),
        base::BindOnce(&SessionStorageImpl::OnCommitResult,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  return map_entry;
}

void SessionStorageImpl::OnDataMapCreation(
    const std::vector<uint8_t>& map_prefix,
    SessionStorageDataMap* map) {
  DCHECK(data_maps_.find(map_prefix) == data_maps_.end());
  data_maps_.emplace(std::piecewise_construct,
                     std::forward_as_tuple(map_prefix),
                     std::forward_as_tuple(map));
}

void SessionStorageImpl::OnDataMapDestruction(
    const std::vector<uint8_t>& map_prefix) {
  data_maps_.erase(map_prefix);
}

void SessionStorageImpl::OnCommitResult(leveldb::Status status) {
  if (connection_state_ == CONNECTION_SHUTDOWN)
    return;

  DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
  UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.CommitResult",
                            leveldb_env::GetLevelDBStatusUMAValue(status),
                            leveldb_env::LEVELDB_STATUS_MAX);
  if (status.ok()) {
    commit_error_count_ = 0;
    return;
  }
  commit_error_count_++;
  if (commit_error_count_ > kSessionStorageCommitErrorThreshold) {
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
    DeleteAndRecreateDatabase(
        "SessionStorageContext.OpenResultAfterCommitErrors");
  }
}

void SessionStorageImpl::OnCommitResultWithCallback(base::OnceClosure callback,
                                                    leveldb::Status status) {
  OnCommitResult(status);
  std::move(callback).Run();
}

scoped_refptr<SessionStorageDataMap>
SessionStorageImpl::MaybeGetExistingDataMapForId(
    const std::vector<uint8_t>& map_number_as_bytes) {
  auto it = data_maps_.find(map_number_as_bytes);
  if (it == data_maps_.end())
    return nullptr;
  return base::WrapRefCounted(it->second);
}

void SessionStorageImpl::RegisterShallowClonedNamespace(
    SessionStorageMetadata::NamespaceEntry source_namespace_entry,
    const std::string& new_namespace_id,
    const SessionStorageNamespaceImpl::StorageKeyAreas& clone_from_areas) {
  std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;

  bool found = false;
  auto it = namespaces_.find(new_namespace_id);
  if (it != namespaces_.end()) {
    found = true;
    if (it->second->IsPopulated()) {
      // Assumes this method is called on a stack handling a mojo message.
      receiver_.ReportBadMessage("Cannot clone to already populated namespace");
      return;
    }
  }

  DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
  auto namespace_entry = metadata_.GetOrCreateNamespaceEntry(new_namespace_id);
  metadata_.RegisterShallowClonedNamespace(source_namespace_entry,
                                           namespace_entry, &save_tasks);
  if (database_) {
    database_->RunBatchDatabaseTasks(
        std::move(save_tasks),
        base::BindOnce(&SessionStorageImpl::OnCommitResult,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (found) {
    it->second->PopulateAsClone(database_.get(), namespace_entry,
                                clone_from_areas);
    return;
  }

  auto namespace_impl = CreateSessionStorageNamespaceImpl(new_namespace_id);
  namespace_impl->PopulateAsClone(database_.get(), namespace_entry,
                                  clone_from_areas);
  namespaces_.emplace(std::piecewise_construct,
                      std::forward_as_tuple(new_namespace_id),
                      std::forward_as_tuple(std::move(namespace_impl)));
}

std::unique_ptr<SessionStorageNamespaceImpl>
SessionStorageImpl::CreateSessionStorageNamespaceImpl(
    std::string namespace_id) {
  SessionStorageAreaImpl::RegisterNewAreaMap map_id_callback =
      base::BindRepeating(&SessionStorageImpl::RegisterNewAreaMap,
                          base::Unretained(this));

  return std::make_unique<SessionStorageNamespaceImpl>(
      std::move(namespace_id), this, std::move(map_id_callback), this);
}

void SessionStorageImpl::DoDatabaseDelete(const std::string& namespace_id) {
  DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
  std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> tasks;
  metadata_.DeleteNamespace(namespace_id, &tasks);
  if (database_) {
    database_->RunBatchDatabaseTasks(
        std::move(tasks), base::BindOnce(&SessionStorageImpl::OnCommitResult,
                                         weak_ptr_factory_.GetWeakPtr()));
  }
}

void SessionStorageImpl::RunWhenConnected(base::OnceClosure callback) {
  switch (connection_state_) {
    case NO_CONNECTION:
      // If we don't have a filesystem_connection_, we'll need to establish one.
      connection_state_ = CONNECTION_IN_PROGRESS;
      receiver_.Pause();
      on_database_opened_callbacks_.push_back(std::move(callback));
      InitiateConnection();
      return;
    case CONNECTION_IN_PROGRESS:
      // Queue this OpenSessionStorage call for when we have a level db pointer.
      on_database_opened_callbacks_.push_back(std::move(callback));
      return;
    case CONNECTION_SHUTDOWN:
      NOTREACHED_IN_MIGRATION();
      return;
    case CONNECTION_FINISHED:
      std::move(callback).Run();
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void SessionStorageImpl::InitiateConnection(bool in_memory_only) {
  DCHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);

  if (backing_mode_ != BackingMode::kNoDisk && !in_memory_only &&
      !partition_directory_.empty()) {
    // We were given a subdirectory to write to, so use a disk backed database.
    if (backing_mode_ == BackingMode::kClearDiskStateOnOpen) {
      DomStorageDatabase::Destroy(partition_directory_, database_name_,
                                  database_task_runner_, base::DoNothing());
    }

    in_memory_ = false;
    database_ = AsyncDomStorageDatabase::OpenDirectory(
        partition_directory_, database_name_, memory_dump_id_,
        database_task_runner_,
        base::BindOnce(&SessionStorageImpl::OnDatabaseOpened,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // We were not given a subdirectory. Use a memory backed database.
  in_memory_ = true;
  database_ = AsyncDomStorageDatabase::OpenInMemory(
      memory_dump_id_, "SessionStorageDatabase", database_task_runner_,
      base::BindOnce(&SessionStorageImpl::OnDatabaseOpened,
                     weak_ptr_factory_.GetWeakPtr()));
}

SessionStorageImpl::ValueAndStatus::ValueAndStatus() = default;

SessionStorageImpl::ValueAndStatus::ValueAndStatus(ValueAndStatus&&) = default;

SessionStorageImpl::ValueAndStatus::~ValueAndStatus() = default;

SessionStorageImpl::KeyValuePairsAndStatus::KeyValuePairsAndStatus() = default;

SessionStorageImpl::KeyValuePairsAndStatus::KeyValuePairsAndStatus(
    KeyValuePairsAndStatus&&) = default;

SessionStorageImpl::KeyValuePairsAndStatus::~KeyValuePairsAndStatus() = default;

void SessionStorageImpl::OnDatabaseOpened(leveldb::Status status) {
  if (!status.ok()) {
    UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.DatabaseOpenError",
                              leveldb_env::GetLevelDBStatusUMAValue(status),
                              leveldb_env::LEVELDB_STATUS_MAX);
    if (in_memory_) {
      UMA_HISTOGRAM_ENUMERATION(
          "SessionStorageContext.DatabaseOpenError.Memory",
          leveldb_env::GetLevelDBStatusUMAValue(status),
          leveldb_env::LEVELDB_STATUS_MAX);
    } else {
      UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.DatabaseOpenError.Disk",
                                leveldb_env::GetLevelDBStatusUMAValue(status),
                                leveldb_env::LEVELDB_STATUS_MAX);
    }
    LogDatabaseOpenResult(OpenResult::kDatabaseOpenFailed);
    // If we failed to open the database, try to delete and recreate the
    // database, or ultimately fallback to an in-memory database.
    DeleteAndRecreateDatabase(
        "SessionStorageContext.OpenResultAfterOpenFailed");
    return;
  }

  if (!database_) {
    // Some tests only simulate database connection without a database being
    // present.
    OnConnectionFinished();
    return;
  }

  database_->RunDatabaseTask(
      base::BindOnce([](const DomStorageDatabase& db) {
        ValueAndStatus version;
        version.status = db.Get(
            base::make_span(SessionStorageMetadata::kDatabaseVersionBytes),
            &version.value);

        KeyValuePairsAndStatus namespaces;
        namespaces.status = db.GetPrefixed(
            base::make_span(SessionStorageMetadata::kNamespacePrefixBytes),
            &namespaces.key_value_pairs);

        ValueAndStatus next_map_id;
        next_map_id.status =
            db.Get(base::make_span(SessionStorageMetadata::kNextMapIdKeyBytes),
                   &next_map_id.value);

        return std::make_tuple(std::move(version), std::move(namespaces),
                               std::move(next_map_id));
      }),
      base::BindOnce(&SessionStorageImpl::OnGotDatabaseMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SessionStorageImpl::OnGotDatabaseMetadata(
    ValueAndStatus version,
    KeyValuePairsAndStatus namespaces,
    ValueAndStatus next_map_id) {
  if (connection_state_ == CONNECTION_SHUTDOWN)
    return;

  std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> migration_tasks;

  MetadataParseResult version_parse =
      ParseDatabaseVersion(std::move(version), &migration_tasks);
  if (version_parse.open_result != OpenResult::kSuccess) {
    LogDatabaseOpenResult(version_parse.open_result);
    DeleteAndRecreateDatabase(version_parse.histogram_name);
    return;
  }

  MetadataParseResult namespaces_parse =
      ParseNamespaces(std::move(namespaces), std::move(migration_tasks));
  if (namespaces_parse.open_result != OpenResult::kSuccess) {
    LogDatabaseOpenResult(namespaces_parse.open_result);
    DeleteAndRecreateDatabase(namespaces_parse.histogram_name);
    return;
  }

  MetadataParseResult next_map_id_parse =
      ParseNextMapId(std::move(next_map_id));
  if (next_map_id_parse.open_result != OpenResult::kSuccess) {
    LogDatabaseOpenResult(next_map_id_parse.open_result);
    DeleteAndRecreateDatabase(next_map_id_parse.histogram_name);
    return;
  }

  OnConnectionFinished();
}

SessionStorageImpl::MetadataParseResult
SessionStorageImpl::ParseDatabaseVersion(
    ValueAndStatus version,
    std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>* migration_tasks) {
  if (version.status.ok()) {
    if (!metadata_.ParseDatabaseVersion(std::move(version.value),
                                        migration_tasks)) {
      return {OpenResult::kInvalidVersion,
              "SessionStorageContext.OpenResultAfterInvalidVersion"};
    }
    database_initialized_ = true;
    return {OpenResult::kSuccess, ""};
  }

  if (version.status.IsNotFound()) {
    // treat as v0 or new database
    metadata_.ParseDatabaseVersion(std::nullopt, migration_tasks);
    return {OpenResult::kSuccess, ""};
  }

  // Other read error, Possibly database corruption
  UMA_HISTOGRAM_ENUMERATION(
      "SessionStorageContext.ReadVersionError",
      leveldb_env::GetLevelDBStatusUMAValue(version.status),
      leveldb_env::LEVELDB_STATUS_MAX);
  return {OpenResult::kVersionReadError,
          "SessionStorageContext.OpenResultAfterReadVersionError"};
}

SessionStorageImpl::MetadataParseResult SessionStorageImpl::ParseNamespaces(
    KeyValuePairsAndStatus namespaces,
    std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> migration_tasks) {
  DCHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);

  if (!namespaces.status.ok()) {
    UMA_HISTOGRAM_ENUMERATION(
        "SessionStorageContext.ReadNamespacesError",
        leveldb_env::GetLevelDBStatusUMAValue(namespaces.status),
        leveldb_env::LEVELDB_STATUS_MAX);
    return {OpenResult::kNamespacesReadError,
            "SessionStorageContext.OpenResultAfterReadNamespacesError"};
  }

  bool parsing_success = metadata_.ParseNamespaces(
      std::move(namespaces.key_value_pairs), &migration_tasks);

  if (!parsing_success) {
    UMA_HISTOGRAM_ENUMERATION(
        "SessionStorageContext.ReadNamespacesError",
        leveldb_env::GetLevelDBStatusUMAValue(leveldb::Status::OK()),
        leveldb_env::LEVELDB_STATUS_MAX);
    return {OpenResult::kNamespacesReadError,
            "SessionStorageContext.OpenResultAfterReadNamespacesError"};
  }

  if (!migration_tasks.empty()) {
    // In tests this write may happen synchronously, which is problematic since
    // the OnCommitResult callback can be invoked before the database is fully
    // initialized. There's no harm in deferring in other situations, so we just
    // always defer here.
    database_->RunBatchDatabaseTasks(
        std::move(migration_tasks),
        base::BindOnce(
            [](base::OnceCallback<void(leveldb::Status)> callback,
               scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
               leveldb::Status status) {
              callback_task_runner->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback), status));
            },
            base::BindOnce(&SessionStorageImpl::OnCommitResult,
                           weak_ptr_factory_.GetWeakPtr()),
            base::SequencedTaskRunner::GetCurrentDefault()));
  }

  return {OpenResult::kSuccess, ""};
}

SessionStorageImpl::MetadataParseResult SessionStorageImpl::ParseNextMapId(
    ValueAndStatus next_map_id) {
  if (!next_map_id.status.ok()) {
    if (next_map_id.status.IsNotFound())
      return {OpenResult::kSuccess, ""};

    // Other read error. Possibly database corruption.
    UMA_HISTOGRAM_ENUMERATION(
        "SessionStorageContext.ReadNextMapIdError",
        leveldb_env::GetLevelDBStatusUMAValue(next_map_id.status),
        leveldb_env::LEVELDB_STATUS_MAX);
    return {OpenResult::kNamespacesReadError,
            "SessionStorageContext.OpenResultAfterReadNextMapIdError"};
  }

  metadata_.ParseNextMapId(std::move(next_map_id.value));
  return {OpenResult::kSuccess, ""};
}

void SessionStorageImpl::OnConnectionFinished() {
  if (connection_state_ == CONNECTION_SHUTDOWN)
    return;

  DCHECK(!database_ || connection_state_ == CONNECTION_IN_PROGRESS);

  // If connection was opened successfully, reset tried_to_recreate_during_open_
  // to enable recreating the database on future errors.
  if (database_)
    tried_to_recreate_during_open_ = false;

  LogDatabaseOpenResult(OpenResult::kSuccess);
  open_result_histogram_ = nullptr;

  // |database_| should be known to either be valid or invalid by now. Run our
  // delayed bindings.
  connection_state_ = CONNECTION_FINISHED;
  receiver_.Resume();
  std::vector<base::OnceClosure> callbacks;
  std::swap(callbacks, on_database_opened_callbacks_);
  for (size_t i = 0; i < callbacks.size(); ++i)
    std::move(callbacks[i]).Run();
}

void SessionStorageImpl::PurgeAllNamespaces() {
  for (const auto& it : data_maps_)
    it.second->storage_area()->CancelAllPendingRequests();
  for (const auto& namespace_pair : namespaces_)
    namespace_pair.second->Reset();
  DCHECK(data_maps_.empty());
}

void SessionStorageImpl::DeleteAndRecreateDatabase(const char* histogram_name) {
  if (connection_state_ == CONNECTION_SHUTDOWN)
    return;

  // We're about to set database_ to null, so delete the StorageAreas
  // that might still be using the old database.
  PurgeAllNamespaces();

  // Reset state to be in process of connecting. This will cause requests for
  // StorageAreas to be queued until the connection is complete.
  connection_state_ = CONNECTION_IN_PROGRESS;
  receiver_.Pause();
  commit_error_count_ = 0;
  database_.reset();
  open_result_histogram_ = histogram_name;

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

  protected_namespaces_from_scavenge_.clear();

  // Destroy database, and try again.
  if (!in_memory_) {
    DomStorageDatabase::Destroy(
        partition_directory_, database_name_, database_task_runner_,
        base::BindOnce(&SessionStorageImpl::OnDBDestroyed,
                       weak_ptr_factory_.GetWeakPtr(), recreate_in_memory));
  } else {
    // No directory, so nothing to destroy. Retrying to recreate will probably
    // fail, but try anyway.
    InitiateConnection(recreate_in_memory);
  }
}

void SessionStorageImpl::OnDBDestroyed(bool recreate_in_memory,
                                       leveldb::Status status) {
  UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.DestroyDBResult",
                            leveldb_env::GetLevelDBStatusUMAValue(status),
                            leveldb_env::LEVELDB_STATUS_MAX);
  // We're essentially ignoring the status here. Even if destroying failed we
  // still want to go ahead and try to recreate.
  InitiateConnection(recreate_in_memory);
}

void SessionStorageImpl::OnShutdownComplete() {
  DCHECK(shutdown_complete_callback_);
  // Flush any final tasks on the DB task runner before invoking the callback.
  PurgeAllNamespaces();
  database_.reset();
  database_task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(), std::move(shutdown_complete_callback_));
}

void SessionStorageImpl::GetStatistics(size_t* total_cache_size,
                                       size_t* unused_area_count) {
  *total_cache_size = 0;
  *unused_area_count = 0;
  for (const auto& it : data_maps_) {
    *total_cache_size += it.second->storage_area()->memory_used();
    if (it.second->binding_count() == 0)
      (*unused_area_count)++;
  }
}

void SessionStorageImpl::LogDatabaseOpenResult(OpenResult result) {
  if (result != OpenResult::kSuccess) {
    UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.OpenError", result);
  }
  if (open_result_histogram_) {
    base::UmaHistogramEnumeration(open_result_histogram_, result);
  }
}

}  // namespace storage
