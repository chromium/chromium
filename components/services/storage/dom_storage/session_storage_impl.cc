// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_impl.h"

#include <inttypes.h>

#include <utility>

#include "base/barrier_closure.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
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
      NOTREACHED();
  }
}

}  // namespace

SessionStorageImpl::SessionStorageImpl(
    const base::FilePath& storage_partition_directory,
    BackingMode backing_mode,
    DestructSessionStorageCallback destruct_callback,
    mojo::PendingReceiver<mojom::SessionStorageControl> receiver)
    : destruct_callback_(std::move(destruct_callback)),
      backing_mode_(backing_mode),
      storage_partition_directory_(storage_partition_directory),
      memory_dump_id_(base::StringPrintf("SessionStorage/0x%" PRIXPTR,
                                         reinterpret_cast<uintptr_t>(this))),
      receiver_(this, std::move(receiver)) {
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "SessionStorage",
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::trace_event::MemoryDumpProvider::Options());
  receiver_.set_disconnect_handler(
      base::BindOnce(&SessionStorageImpl::OnReceiverDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

SessionStorageImpl::~SessionStorageImpl() {
  ShutDown();
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void SessionStorageImpl::BindNamespace(
    const std::string& namespace_id,
    mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&SessionStorageImpl::BindNamespace,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    namespace_id, std::move(receiver)));
    return;
  }
  auto found = namespaces_.find(namespace_id);
  if (found == namespaces_.end()) {
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
}

void SessionStorageImpl::BindStorageArea(
    const blink::StorageKey& storage_key,
    const std::string& namespace_id,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&SessionStorageImpl::BindStorageArea,
                                    weak_ptr_factory_.GetWeakPtr(), storage_key,
                                    namespace_id, std::move(receiver)));
    return;
  }

  auto found = namespaces_.find(namespace_id);
  if (found == namespaces_.end()) {
    return;
  }

  SessionStorageMetadata::NamespaceEntry namespace_entry =
      metadata_.GetOrCreateNamespaceEntry(namespace_id);

  if (found->second->state() ==
      SessionStorageNamespaceImpl::State::kNotPopulated) {
    found->second->PopulateFromMetadata(database_.get(), namespace_entry);
  }

  PurgeUnusedAreasIfNeeded();
  found->second->OpenArea(storage_key, std::move(receiver), namespace_entry);
}

void SessionStorageImpl::CreateNamespace(const std::string& namespace_id) {
  if (namespaces_.find(namespace_id) != namespaces_.end())
    return;

  namespaces_.emplace(std::make_pair(
      namespace_id, CreateSessionStorageNamespaceImpl(namespace_id)));
}

void SessionStorageImpl::CloneNamespace(
    const std::string& clone_from_namespace_id,
    const std::string& clone_to_namespace_id,
    mojom::SessionStorageCloneType clone_type) {
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
      } else if (metadata_.namespace_storage_key_map().contains(
                     clone_from_namespace_id)) {
        CHECK_EQ(connection_state_, CONNECTION_FINISHED);

        // The namespace exists on disk but is not in-use, so do the appropriate
        // metadata operations to clone the namespace and set up the new object.
        auto source_namespace_entry =
            metadata_.GetOrCreateNamespaceEntry(clone_from_namespace_id);
        auto namespace_entry =
            metadata_.GetOrCreateNamespaceEntry(clone_to_namespace_id);
        metadata_.RegisterShallowClonedNamespace(source_namespace_entry,
                                                 namespace_entry);
        if (database_) {
          database_->PutMetadata(
              SessionStorageMetadata::ToDomStorageMetadata(namespace_entry),
              base::BindOnce(&SessionStorageImpl::OnCommitResult,
                             weak_ptr_factory_.GetWeakPtr()));
        }
      }
      // If there is no sign of a source namespace, just run with an empty
      // namespace.
      break;
    default:
      NOTREACHED();
  }
  namespaces_.emplace(
      std::piecewise_construct, std::forward_as_tuple(clone_to_namespace_id),
      std::forward_as_tuple(std::move(clone_to_namespace_impl)));
}

void SessionStorageImpl::DeleteNamespace(const std::string& namespace_id,
                                         bool should_persist) {
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
    RunWhenConnected(base::BindOnce(
        &SessionStorageImpl::DeleteNamespacesFromMetadataAndDatabase,
        weak_ptr_factory_.GetWeakPtr(),
        std::vector<std::string>({namespace_id})));
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
    return;
  }

  // If we don't have the namespace loaded, then we can delete it all using the
  // metadata.
  scoped_refptr<DomStorageDatabase::SharedMapLocator> map_locator =
      metadata_.TakeExistingMap(namespace_id, storage_key);
  if (!map_locator || !database_) {
    // Nothing to delete.
    std::move(callback).Run();
    return;
  }

  // Delete `storage_key` from `namespace_id` in the database.  Also delete
  // `map_locator` when not referenced by a cloned session.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  if (map_locator->session_ids().empty()) {
    maps_to_delete.emplace_back(std::move(*map_locator));
  }
  database_->DeleteStorageKeysFromSession(
      namespace_id, /*metadata_to_delete=*/{storage_key},
      std::move(maps_to_delete),
      base::BindOnce(&SessionStorageImpl::OnCommitResult,
                     weak_ptr_factory_.GetWeakPtr())
          .Then(std::move(callback)));
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
    database_->RewriteDB(base::IgnoreArgs<DbStatus>(std::move(callback)));
  } else {
    std::move(callback).Run();
  }
}

void SessionStorageImpl::ShutDown() {
  receiver_.reset();

  // The namespaces will DCHECK if they are destructed with pending clones. It
  // is valid to drop these on shutdown.
  for (auto& namespace_pair : namespaces_) {
    namespace_pair.second->ClearChildNamespacesWaitingForClone();
  }

  // Nothing to do if no connection to the database was ever finished.
  if (connection_state_ == CONNECTION_FINISHED) {
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
  }

  PurgeAllNamespaces();
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
  else if (base::SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled()) {
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

void SessionStorageImpl::ScavengeUnusedNamespaces() {
  if (has_scavenged_) {
    return;
  }
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(
        base::BindOnce(&SessionStorageImpl::ScavengeUnusedNamespaces,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  has_scavenged_ = true;
  std::vector<std::string> namespaces_to_delete;
  for (const auto& metadata_namespace : metadata_.namespace_storage_key_map()) {
    const std::string& namespace_id = metadata_namespace.first;
    if (namespaces_.contains(namespace_id) ||
        protected_namespaces_from_scavenge_.contains(namespace_id)) {
      continue;
    }
    namespaces_to_delete.push_back(namespace_id);
  }
  DeleteNamespacesFromMetadataAndDatabase(std::move(namespaces_to_delete));
  protected_namespaces_from_scavenge_.clear();
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
    const auto& storage_key = it.second->map_locator().storage_key();
    std::string storage_key_str =
        storage_key.GetMemoryDumpString(/*max_length=*/50);
    std::string area_dump_name = base::StringPrintf(
        "%s/%s/0x%" PRIXPTR, context_name.c_str(), storage_key_str.c_str(),
        reinterpret_cast<uintptr_t>(it.second->storage_area()));
    it.second->storage_area()->OnMemoryDump(area_dump_name, pmd);
  }
  return true;
}

const base::FilePath& SessionStorageImpl::GetStoragePartitionDirectory() const {
  return storage_partition_directory_;
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

base::FilePath SessionStorageImpl::GetDatabasePath() const {
  return DomStorageDatabase::GetPath(StorageType::kSessionStorage,
                                     storage_partition_directory_);
}

scoped_refptr<DomStorageDatabase::SharedMapLocator>
SessionStorageImpl::RegisterNewAreaMap(const std::string& namespace_id,
                                       const blink::StorageKey& storage_key) {
  CHECK_EQ(connection_state_, CONNECTION_FINISHED);

  scoped_refptr<DomStorageDatabase::SharedMapLocator> map_entry =
      metadata_.RegisterNewMap(namespace_id, storage_key);
  if (database_) {
    // Save the new map in the database.
    DomStorageDatabase::Metadata metadata;
    metadata.next_map_id = map_entry->map_id().value() + 1;
    metadata.map_metadata.push_back({
        .map_locator{
            /*session_id=*/namespace_id,
            map_entry->storage_key(),
            map_entry->map_id().value(),
        },
    });
    database_->PutMetadata(std::move(metadata),
                           base::BindOnce(&SessionStorageImpl::OnCommitResult,
                                          weak_ptr_factory_.GetWeakPtr()));
  }
  return map_entry;
}

void SessionStorageImpl::OnDataMapCreation(int64_t map_id,
                                           SessionStorageDataMap* map) {
  auto result = data_maps_.emplace(std::piecewise_construct,
                                   std::forward_as_tuple(map_id),
                                   std::forward_as_tuple(map));

  // `map_id` must identify a unique new map that did not exist in `data_maps_`.
  CHECK(result.second);
}

void SessionStorageImpl::OnDataMapDestruction(int64_t map_id) {
  data_maps_.erase(map_id);
}

void SessionStorageImpl::OnCommitResult(DbStatus status) {
  if (status.ok()) {
    commit_error_count_ = 0;
    return;
  }

  if (connection_state_ != CONNECTION_FINISHED) {
    // Previous commit errors deleted and recreated the database below.  Ignore
    // additional errors from the old database while waiting for the new
    // database to open.
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
    DeleteAndRecreateDatabase();
  }
}

scoped_refptr<SessionStorageDataMap>
SessionStorageImpl::MaybeGetExistingDataMapForId(int64_t map_id) {
  auto it = data_maps_.find(map_id);
  if (it == data_maps_.end())
    return nullptr;
  return base::WrapRefCounted(it->second);
}

void SessionStorageImpl::RegisterShallowClonedNamespace(
    const std::string& source_namespace_id,
    const std::string& new_namespace_id,
    const SessionStorageNamespaceImpl::StorageKeyAreas& clone_from_areas) {
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

  CHECK_EQ(connection_state_, CONNECTION_FINISHED);

  auto source_namespace_entry =
      metadata_.GetOrCreateNamespaceEntry(source_namespace_id);
  auto namespace_entry = metadata_.GetOrCreateNamespaceEntry(new_namespace_id);
  metadata_.RegisterShallowClonedNamespace(source_namespace_entry,
                                           namespace_entry);

  if (database_) {
    database_->PutMetadata(
        SessionStorageMetadata::ToDomStorageMetadata(namespace_entry),
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

void SessionStorageImpl::DeleteNamespacesFromMetadataAndDatabase(
    std::vector<std::string> namespace_ids) {
  CHECK_EQ(connection_state_, CONNECTION_FINISHED);

  // Remove each namespace from `metadata_`.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  for (const std::string& namespace_id : namespace_ids) {
    std::map<blink::StorageKey,
             scoped_refptr<DomStorageDatabase::SharedMapLocator>>
        namespace_to_delete = metadata_.TakeNamespace(namespace_id);

    // Find unreferenced map key/value pairs to delete from `database_`.
    for (auto& [storage_key, map_locator] : namespace_to_delete) {
      if (map_locator->session_ids().empty()) {
        maps_to_delete.emplace_back(std::move(*map_locator));
      }
    }
  }

  // Delete the namespaces and map key/values from `database_`.
  if (!database_) {
    return;
  }
  database_->DeleteSessions(std::move(namespace_ids), std::move(maps_to_delete),
                            base::BindOnce(&SessionStorageImpl::OnCommitResult,
                                           weak_ptr_factory_.GetWeakPtr()));
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
    case CONNECTION_FINISHED:
      std::move(callback).Run();
      return;
  }
  NOTREACHED();
}

void SessionStorageImpl::InitiateConnection(bool in_memory_only) {
  CHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);

  if (backing_mode_ != BackingMode::kNoDisk && !in_memory_only &&
      !storage_partition_directory_.empty()) {
    // We were given a subdirectory to write to, so use a disk backed database.
    if (backing_mode_ == BackingMode::kClearDiskStateOnOpen) {
      DomStorageDatabaseFactory::Destroy(GetDatabasePath(), base::DoNothing());
    }

    in_memory_ = false;
    database_ = AsyncDomStorageDatabase::Open(
        StorageType::kSessionStorage, GetDatabasePath(), memory_dump_id_,
        base::BindOnce(&SessionStorageImpl::OnDatabaseOpened,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // We were not given a subdirectory. Use a memory backed database.
  in_memory_ = true;
  database_ = AsyncDomStorageDatabase::Open(
      StorageType::kSessionStorage,
      /*database_path=*/base::FilePath(), memory_dump_id_,
      base::BindOnce(&SessionStorageImpl::OnDatabaseOpened,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SessionStorageImpl::OnDatabaseOpened(DbStatus status) {
  if (!status.ok()) {
    // If we failed to open the database, try to delete and recreate the
    // database, or ultimately fallback to an in-memory database.
    DeleteAndRecreateDatabase();
    return;
  }

  if (!database_) {
    // Some tests only simulate database connection without a database being
    // present.
    OnConnectionFinished();
    return;
  }

  database_->ReadAllMetadata(
      base::BindOnce(&SessionStorageImpl::OnGotDatabaseMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SessionStorageImpl::OnGotDatabaseMetadata(
    StatusOr<DomStorageDatabase::Metadata> all_metadata) {
  if (!all_metadata.has_value()) {
    DeleteAndRecreateDatabase();
    return;
  }

  metadata_.Initialize(*std::move(all_metadata));

  OnConnectionFinished();
}

void SessionStorageImpl::OnConnectionFinished() {
  CHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);

  // If connection was opened successfully, reset tried_to_recreate_during_open_
  // to enable recreating the database on future errors.
  if (database_)
    tried_to_recreate_during_open_ = false;

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

void SessionStorageImpl::DeleteAndRecreateDatabase() {
  // We're about to set database_ to null, so delete the StorageAreas
  // that might still be using the old database.
  PurgeAllNamespaces();

  // Reset state to be in process of connecting. This will cause requests for
  // StorageAreas to be queued until the connection is complete.
  connection_state_ = CONNECTION_IN_PROGRESS;
  receiver_.Pause();
  commit_error_count_ = 0;
  database_.reset();

  bool recreate_in_memory = false;

  // If tried to recreate database on disk already, try again but this time
  // in memory.
  if (tried_to_recreate_during_open_) {
    if (in_memory_) {
      // Give up completely, run without any database.
      OnConnectionFinished();
      return;
    }
    recreate_in_memory = true;
  }

  tried_to_recreate_during_open_ = true;

  protected_namespaces_from_scavenge_.clear();

  // Destroy database, and try again.
  if (!in_memory_) {
    DomStorageDatabaseFactory::Destroy(
        GetDatabasePath(),
        base::BindOnce(&SessionStorageImpl::OnDBDestroyed,
                       weak_ptr_factory_.GetWeakPtr(), recreate_in_memory));
  } else {
    // No directory, so nothing to destroy. Retrying to recreate will probably
    // fail, but try anyway.
    InitiateConnection(recreate_in_memory);
  }
}

void SessionStorageImpl::OnDBDestroyed(bool recreate_in_memory,
                                       DbStatus status) {
  // We're essentially ignoring the status here. Even if destroying failed we
  // still want to go ahead and try to recreate.
  InitiateConnection(recreate_in_memory);
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

void SessionStorageImpl::OnReceiverDisconnected() {
  std::move(destruct_callback_).Run(this);
}

}  // namespace storage
