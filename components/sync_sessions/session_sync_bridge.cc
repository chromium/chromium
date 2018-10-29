// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_sync_bridge.h"

#include <stdint.h>

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/time.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/in_memory_metadata_change_list.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"

namespace sync_sessions {
namespace {

using sync_pb::SessionSpecifics;
using syncer::MetadataChangeList;
using syncer::ModelTypeStore;
using syncer::ModelTypeSyncBridge;

// Maximum number of favicons to sync.
const int kMaxSyncFavicons = 200;

// Default time without activity after which a session is considered stale and
// becomes a candidate for garbage collection.
const base::TimeDelta kStaleSessionThreshold = base::TimeDelta::FromDays(14);

std::unique_ptr<syncer::EntityData> MoveToEntityData(
    const std::string& client_name,
    SessionSpecifics* specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->non_unique_name = client_name;
  entity_data->specifics.mutable_session()->Swap(specifics);
  return entity_data;
}

class LocalSessionWriteBatch : public LocalSessionEventHandlerImpl::WriteBatch {
 public:
  LocalSessionWriteBatch(const SessionStore::SessionInfo& session_info,
                         std::unique_ptr<SessionStore::WriteBatch> batch,
                         syncer::ModelTypeChangeProcessor* processor)
      : session_info_(session_info),
        batch_(std::move(batch)),
        processor_(processor) {
    DCHECK(batch_);
    DCHECK(processor_);
    DCHECK(processor_->IsTrackingMetadata());
  }

  ~LocalSessionWriteBatch() override {}

  // WriteBatch implementation.
  void Delete(int tab_node_id) override {
    const std::string storage_key =
        batch_->DeleteLocalTabWithoutUpdatingTracker(tab_node_id);
    processor_->Delete(storage_key, batch_->GetMetadataChangeList());
  }

  void Put(std::unique_ptr<sync_pb::SessionSpecifics> specifics) override {
    DCHECK(SessionStore::AreValidSpecifics(*specifics));
    const std::string storage_key =
        batch_->PutWithoutUpdatingTracker(*specifics);

    processor_->Put(
        storage_key,
        MoveToEntityData(session_info_.client_name, specifics.get()),
        batch_->GetMetadataChangeList());
  }

  void Commit() override {
    DCHECK(batch_) << "Cannot commit twice";
    SessionStore::WriteBatch::Commit(std::move(batch_));
  }

 private:
  const SessionStore::SessionInfo session_info_;
  std::unique_ptr<SessionStore::WriteBatch> batch_;
  syncer::ModelTypeChangeProcessor* const processor_;
};

}  // namespace

SessionSyncBridge::SessionSyncBridge(
    SyncSessionsClient* sessions_client,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)),
      sessions_client_(sessions_client),
      local_session_event_router_(
          sessions_client->GetLocalSessionEventRouter()),
      favicon_cache_(sessions_client->GetFaviconService(),
                     sessions_client->GetHistoryService(),
                     kMaxSyncFavicons),
      session_store_factory_(SessionStore::CreateFactory(
          sessions_client,
          base::BindRepeating(&FaviconCache::UpdateMappingsFromForeignTab,
                              base::Unretained(&favicon_cache_)))),
      weak_ptr_factory_(this) {
  DCHECK(sessions_client_);
  DCHECK(local_session_event_router_);
}

SessionSyncBridge::~SessionSyncBridge() {
  if (syncing_) {
    local_session_event_router_->Stop();
  }
}

void SessionSyncBridge::ScheduleGarbageCollection() {
  if (!syncing_) {
    return;
  }
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SessionSyncBridge::DoGarbageCollection,
                                weak_ptr_factory_.GetWeakPtr()));
}

FaviconCache* SessionSyncBridge::GetFaviconCache() {
  return &favicon_cache_;
}

SessionsGlobalIdMapper* SessionSyncBridge::GetGlobalIdMapper() {
  return &global_id_mapper_;
}

OpenTabsUIDelegate* SessionSyncBridge::GetOpenTabsUIDelegate() {
  if (!syncing_) {
    return nullptr;
  }
  return syncing_->open_tabs_ui_delegate.get();
}

syncer::SyncableService* SessionSyncBridge::GetSyncableService() {
  return nullptr;
}

syncer::ModelTypeSyncBridge* SessionSyncBridge::GetModelTypeSyncBridge() {
  return this;
}

std::unique_ptr<MetadataChangeList>
SessionSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

base::Optional<syncer::ModelError> SessionSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK(syncing_);
  DCHECK(change_processor()->IsTrackingMetadata());

  StartLocalSessionEventHandler();

  return ApplySyncChanges(std::move(metadata_change_list),
                          std::move(entity_data));
}

void SessionSyncBridge::StartLocalSessionEventHandler() {
  // We should be ready to propagate local state to sync.
  DCHECK(syncing_);
  DCHECK(!syncing_->local_session_event_handler);
  DCHECK(change_processor()->IsTrackingMetadata());

  syncing_->local_session_event_handler =
      std::make_unique<LocalSessionEventHandlerImpl>(
          /*delegate=*/this, sessions_client_,
          syncing_->store->mutable_tracker());

  // Start processing local changes, which will be propagated to the store as
  // well as the processor.
  local_session_event_router_->StartRoutingTo(
      syncing_->local_session_event_handler.get());
}

base::Optional<syncer::ModelError> SessionSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK(change_processor()->IsTrackingMetadata());
  DCHECK(syncing_);

  // Merging sessions is simple: remote entities are expected to be foreign
  // sessions (identified by the session tag)  and hence must simply be
  // stored (server wins, including undeletion). For local sessions, remote
  // information is ignored (local wins).
  std::unique_ptr<SessionStore::WriteBatch> batch =
      CreateSessionStoreWriteBatch();
  for (const syncer::EntityChange& change : entity_changes) {
    switch (change.type()) {
      case syncer::EntityChange::ACTION_DELETE:
        // Deletions are all or nothing (since we only ever delete entire
        // sessions). Therefore we don't care if it's a tab node or meta node,
        // and just ensure we've disassociated.
        if (syncing_->store->StorageKeyMatchesLocalSession(
                change.storage_key())) {
          // Another client has attempted to delete our local data (possibly by
          // error or a clock is inaccurate). Just ignore the deletion for now.
          DLOG(WARNING) << "Local session data deleted. Ignoring until next "
                        << "local navigation event.";
          syncing_->local_data_out_of_sync = true;
        } else {
          batch->DeleteForeignEntityAndUpdateTracker(change.storage_key());
        }
        break;
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        const SessionSpecifics& specifics = change.data().specifics.session();

        if (syncing_->store->StorageKeyMatchesLocalSession(
                change.storage_key())) {
          // We should only ever receive a change to our own machine's session
          // info if encryption was turned on. In that case, the data is still
          // the same, so we can ignore.
          DLOG(WARNING) << "Dropping modification to local session.";
          syncing_->local_data_out_of_sync = true;
          continue;
        }

        if (!SessionStore::AreValidSpecifics(specifics) ||
            change.data().client_tag_hash !=
                GenerateSyncableHash(syncer::SESSIONS,
                                     SessionStore::GetClientTag(specifics))) {
          continue;
        }

        batch->PutAndUpdateTracker(specifics, change.data().modification_time);
        // If a favicon or favicon urls are present, load the URLs and visit
        // times into the in-memory favicon cache.
        if (specifics.has_tab()) {
          favicon_cache_.UpdateMappingsFromForeignTab(
              specifics.tab(), change.data().modification_time);
        }
        break;
      }
    }
  }

  static_cast<syncer::InMemoryMetadataChangeList*>(metadata_change_list.get())
      ->TransferChangesTo(batch->GetMetadataChangeList());
  SessionStore::WriteBatch::Commit(std::move(batch));

  if (!entity_changes.empty()) {
    sessions_client_->NotifyForeignSessionUpdated();
  }

  return base::nullopt;
}

void SessionSyncBridge::GetData(StorageKeyList storage_keys,
                                DataCallback callback) {
  DCHECK(syncing_);
  std::move(callback).Run(syncing_->store->GetSessionDataForKeys(storage_keys));
}

void SessionSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  DCHECK(syncing_);
  std::move(callback).Run(syncing_->store->GetAllSessionData());
}

std::string SessionSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return SessionStore::GetClientTag(entity_data.specifics.session());
}

std::string SessionSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  if (!SessionStore::AreValidSpecifics(entity_data.specifics.session())) {
    return std::string();
  }
  return SessionStore::GetStorageKey(entity_data.specifics.session());
}

ModelTypeSyncBridge::StopSyncResponse SessionSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  local_session_event_router_->Stop();
  if (syncing_ && delete_metadata_change_list) {
    syncing_->store->DeleteAllDataAndMetadata();
  }
  syncing_.reset();
  return StopSyncResponse::kModelNoLongerReadyToSync;
}

std::unique_ptr<LocalSessionEventHandlerImpl::WriteBatch>
SessionSyncBridge::CreateLocalSessionWriteBatch() {
  DCHECK(syncing_);

  // If a remote client mangled with our local session (typically deleted
  // entities due to garbage collection), we resubmit all local entities at this
  // point (i.e. local changes observed).
  if (syncing_->local_data_out_of_sync) {
    syncing_->local_data_out_of_sync = false;
    // We use PostTask() to avoid interferring with the ongoing handling of
    // local changes that triggered this function.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&SessionSyncBridge::ResubmitLocalSession,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  return std::make_unique<LocalSessionWriteBatch>(
      syncing_->store->local_session_info(), CreateSessionStoreWriteBatch(),
      change_processor());
}

void SessionSyncBridge::TrackLocalNavigationId(base::Time timestamp,
                                               int unique_id) {
  global_id_mapper_.TrackNavigationId(timestamp, unique_id);
}

void SessionSyncBridge::OnPageFaviconUpdated(const GURL& page_url) {
  favicon_cache_.OnPageFaviconUpdated(page_url, base::Time::Now());
}

void SessionSyncBridge::OnFaviconVisited(const GURL& page_url,
                                         const GURL& favicon_url) {
  favicon_cache_.OnFaviconVisited(page_url, favicon_url);
}

void SessionSyncBridge::OnSyncStarting(
    const syncer::DataTypeActivationRequest& request) {
  DCHECK(!syncing_);

  const syncer::DeviceInfo* device_info =
      sessions_client_->GetLocalDeviceInfo();

  // DeviceInfo must be available by the time sync starts, because there's no
  // task posting involved in the sessions controller.
  DCHECK(device_info);
  DCHECK_EQ(device_info->guid(), request.cache_guid);

  session_store_factory_.Run(
      *device_info, base::BindOnce(&SessionSyncBridge::OnStoreInitialized,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void SessionSyncBridge::OnStoreInitialized(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<SessionStore> store,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK(!syncing_);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  DCHECK(store);
  DCHECK(metadata_batch);

  syncing_.emplace();
  syncing_->store = std::move(store);
  syncing_->open_tabs_ui_delegate = std::make_unique<OpenTabsUIDelegateImpl>(
      sessions_client_, syncing_->store->tracker(), &favicon_cache_,
      base::BindRepeating(&SessionSyncBridge::DeleteForeignSessionFromUI,
                          base::Unretained(this)));

  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  // If initial sync was already done, MergeSyncData() will never be called so
  // we need to start syncing local changes.
  if (change_processor()->IsTrackingMetadata()) {
    StartLocalSessionEventHandler();
  }
}

void SessionSyncBridge::DeleteForeignSessionFromUI(const std::string& tag) {
  if (!syncing_) {
    return;
  }

  std::unique_ptr<SessionStore::WriteBatch> batch =
      CreateSessionStoreWriteBatch();
  DeleteForeignSessionWithBatch(tag, batch.get());
  SessionStore::WriteBatch::Commit(std::move(batch));
}

void SessionSyncBridge::DoGarbageCollection() {
  if (!syncing_) {
    return;
  }

  std::unique_ptr<SessionStore::WriteBatch> batch =
      CreateSessionStoreWriteBatch();

  // Iterate through all the sessions and delete any with age older than
  // |kStaleSessionThreshold|.
  for (const auto* session :
       syncing_->store->tracker()->LookupAllForeignSessions(
           SyncedSessionTracker::RAW)) {
    const base::TimeDelta session_age =
        base::Time::Now() - session->modified_time;
    if (session_age > kStaleSessionThreshold) {
      const std::string session_tag = session->session_tag;
      DVLOG(1) << "Found stale session " << session_tag << " with age "
               << session_age.InDays() << " days, deleting.";
      DeleteForeignSessionWithBatch(session_tag, batch.get());
    }
  }

  SessionStore::WriteBatch::Commit(std::move(batch));
}

void SessionSyncBridge::DeleteForeignSessionWithBatch(
    const std::string& session_tag,
    SessionStore::WriteBatch* batch) {
  DCHECK(syncing_);
  DCHECK(change_processor()->IsTrackingMetadata());

  if (session_tag == syncing_->store->local_session_info().session_tag) {
    DLOG(ERROR) << "Attempting to delete local session. This is not currently "
                << "supported.";
    return;
  }

  // Delete tabs.
  for (int tab_node_id :
       syncing_->store->tracker()->LookupTabNodeIds(session_tag)) {
    const std::string tab_storage_key =
        SessionStore::GetTabStorageKey(session_tag, tab_node_id);
    batch->DeleteForeignEntityAndUpdateTracker(tab_storage_key);
    change_processor()->Delete(tab_storage_key, batch->GetMetadataChangeList());
  }

  // Delete header.
  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(session_tag);
  batch->DeleteForeignEntityAndUpdateTracker(header_storage_key);
  change_processor()->Delete(header_storage_key,
                             batch->GetMetadataChangeList());

  sessions_client_->NotifyForeignSessionUpdated();
}

std::unique_ptr<SessionStore::WriteBatch>
SessionSyncBridge::CreateSessionStoreWriteBatch() {
  DCHECK(syncing_);

  return syncing_->store->CreateWriteBatch(base::BindOnce(
      &SessionSyncBridge::ReportError, weak_ptr_factory_.GetWeakPtr()));
}

void SessionSyncBridge::ResubmitLocalSession() {
  if (!syncing_) {
    return;
  }

  std::unique_ptr<SessionStore::WriteBatch> write_batch =
      CreateSessionStoreWriteBatch();
  std::unique_ptr<syncer::DataBatch> read_batch =
      syncing_->store->GetAllSessionData();
  while (read_batch->HasNext()) {
    syncer::KeyAndData key_and_data = read_batch->Next();
    if (syncing_->store->StorageKeyMatchesLocalSession(key_and_data.first)) {
      change_processor()->Put(key_and_data.first,
                              std::move(key_and_data.second),
                              write_batch->GetMetadataChangeList());
    }
  }

  SessionStore::WriteBatch::Commit(std::move(write_batch));
}

void SessionSyncBridge::ReportError(const syncer::ModelError& error) {
  change_processor()->ReportError(error);
}

SessionSyncBridge::SyncingState::SyncingState() {}

SessionSyncBridge::SyncingState::~SyncingState() {}

}  // namespace sync_sessions
