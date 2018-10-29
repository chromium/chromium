// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/sessions_sync_manager.h"

#include <algorithm>
#include <utility>

#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/model/time.h"
#include "components/sync_sessions/local_session_event_router.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/tab_node_pool.h"

using syncer::DeviceInfo;
using syncer::SyncChange;
using syncer::SyncData;

namespace sync_sessions {

namespace {

// Maximum number of favicons to sync.
// TODO(zea): pull this from the server.
const int kMaxSyncFavicons = 200;

// Default number of days without activity after which a session is considered
// stale and becomes a candidate for garbage collection.
const int kDefaultStaleSessionThresholdDays = 14;  // 2 weeks.

std::string TabNodeIdToTag(const std::string& machine_tag, int tab_node_id) {
  CHECK_GT(tab_node_id, TabNodePool::kInvalidTabNodeID)
      << "https://crbug.com/639009";
  return base::StringPrintf("%s %d", machine_tag.c_str(), tab_node_id);
}

std::string TagFromSpecifics(const sync_pb::SessionSpecifics& specifics) {
  if (specifics.has_header()) {
    return specifics.session_tag();
  } else if (specifics.has_tab()) {
    return TabNodeIdToTag(specifics.session_tag(), specifics.tab_node_id());
  } else {
    return std::string();
  }
}

sync_pb::SessionSpecifics SessionTabToSpecifics(
    const sessions::SessionTab& session_tab,
    const std::string& local_tag,
    int tab_node_id) {
  sync_pb::SessionSpecifics specifics;
  SessionTabToSyncData(session_tab).Swap(specifics.mutable_tab());
  specifics.set_session_tag(local_tag);
  specifics.set_tab_node_id(tab_node_id);
  return specifics;
}

void AppendDeletionsForTabNodes(const std::set<int>& tab_node_ids,
                                const std::string& machine_tag,
                                syncer::SyncChangeList* change_output) {
  for (auto it = tab_node_ids.begin(); it != tab_node_ids.end(); ++it) {
    change_output->push_back(syncer::SyncChange(
        FROM_HERE, SyncChange::ACTION_DELETE,
        SyncData::CreateLocalDelete(TabNodeIdToTag(machine_tag, *it),
                                    syncer::SESSIONS)));
  }
}

class SyncChangeListWriteBatch
    : public LocalSessionEventHandlerImpl::WriteBatch {
 public:
  SyncChangeListWriteBatch(
      const std::string& machine_tag,
      const std::string& session_name,
      const std::set<int>& known_tab_node_ids,
      base::OnceCallback<void(const syncer::SyncChangeList&)> commit_cb)
      : machine_tag_(machine_tag),
        session_name_(session_name),
        known_tab_node_ids_(known_tab_node_ids),
        commit_cb_(std::move(commit_cb)) {}

  syncer::SyncChangeList* sync_change_list() { return &changes_; }

  void PutWithType(std::unique_ptr<sync_pb::SessionSpecifics> specifics,
                   SyncChange::SyncChangeType change_type) {
    sync_pb::EntitySpecifics entity_specifics;
    specifics->Swap(entity_specifics.mutable_session());
    changes_.push_back(SyncChange(
        FROM_HERE, change_type,
        SyncData::CreateLocalData(TagFromSpecifics(entity_specifics.session()),
                                  session_name_, entity_specifics)));
  }

  // WriteBatch implementation.
  void Delete(int tab_node_id) override {
    changes_.push_back(syncer::SyncChange(
        FROM_HERE, SyncChange::ACTION_DELETE,
        SyncData::CreateLocalDelete(TabNodeIdToTag(machine_tag_, tab_node_id),
                                    syncer::SESSIONS)));
  }

  void Put(std::unique_ptr<sync_pb::SessionSpecifics> specifics) override {
    bool new_entity =
        specifics->tab_node_id() != TabNodePool::kInvalidTabNodeID &&
        known_tab_node_ids_.insert(specifics->tab_node_id()).second;

    PutWithType(std::move(specifics), new_entity ? SyncChange::ACTION_ADD
                                                 : SyncChange::ACTION_UPDATE);
  }

  void Commit() override { std::move(commit_cb_).Run(changes_); }

 private:
  const std::string machine_tag_;
  const std::string session_name_;
  std::set<int> known_tab_node_ids_;
  base::OnceCallback<void(const syncer::SyncChangeList&)> commit_cb_;
  syncer::SyncChangeList changes_;
};

}  // namespace

// |local_device| is owned by ProfileSyncService, its lifetime exceeds
// lifetime of SessionSyncManager.
SessionsSyncManager::SessionsSyncManager(
    sync_sessions::SyncSessionsClient* sessions_client)
    : sessions_client_(sessions_client),
      session_tracker_(sessions_client),
      favicon_cache_(sessions_client->GetFaviconService(),
                     sessions_client->GetHistoryService(),
                     kMaxSyncFavicons),
      open_tabs_ui_delegate_(
          sessions_client,
          &session_tracker_,
          &favicon_cache_,
          base::BindRepeating(&SessionsSyncManager::DeleteForeignSessionFromUI,
                              base::Unretained(this))),
      local_tab_pool_out_of_sync_(true),
      stale_session_threshold_days_(kDefaultStaleSessionThresholdDays) {}

SessionsSyncManager::~SessionsSyncManager() {}

// Returns the GUID-based string that should be used for
// |SessionsSyncManager::current_machine_tag_|.
static std::string BuildMachineTag(const std::string& cache_guid) {
  std::string machine_tag = "session_sync";
  machine_tag.append(cache_guid);
  return machine_tag;
}

void SessionsSyncManager::ScheduleGarbageCollection() {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SessionsSyncManager::DoGarbageCollection,
                                base::AsWeakPtr(this)));
}

syncer::SyncableService* SessionsSyncManager::GetSyncableService() {
  return this;
}

syncer::ModelTypeSyncBridge* SessionsSyncManager::GetModelTypeSyncBridge() {
  return nullptr;
}

syncer::SyncMergeResult SessionsSyncManager::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> error_handler) {
  syncer::SyncMergeResult merge_result(type);
  DCHECK(session_tracker_.Empty());
  DCHECK(!local_session_event_handler_);

  error_handler_ = std::move(error_handler);
  sync_processor_ = std::move(sync_processor);

  // SessionDataTypeController ensures that the local device info
  // is available before activating this datatype.
  const DeviceInfo* local_device_info = sessions_client_->GetLocalDeviceInfo();
  if (!local_device_info) {
    merge_result.set_error(error_handler_->CreateAndUploadError(
        FROM_HERE, "Failed to get local device info."));
    return merge_result;
  }

  current_session_name_ = local_device_info->client_name();

  // It's possible(via RebuildAssociations) for lost_navigations_recorder_ to
  // persist between sync being stopped and started. If it did persist, it's
  // already associated with |sync_processor|, so leave it alone.
  if (!lost_navigations_recorder_.get()) {
    lost_navigations_recorder_ =
        std::make_unique<sync_sessions::LostNavigationsRecorder>();
    sync_processor_->AddLocalChangeObserver(lost_navigations_recorder_.get());
  }

  // Make sure we have a machine tag.  We do this now (versus earlier) as it's
  // a conveniently safe time to assert sync is ready and the cache_guid is
  // initialized.
  if (current_machine_tag_.empty()) {
    InitializeCurrentMachineTag(local_device_info->guid());
  }

  session_tracker_.InitLocalSession(current_machine_tag_, current_session_name_,
                                    local_device_info->device_type());

  // TODO(crbug.com/681921): Revisit the somewhat ugly use below of
  // SyncChangeListWriteBatch. Ideally InitFromSyncModel() could use the
  // WriteBatch API as well.
  SyncChangeListWriteBatch batch(current_machine_tag(), current_session_name_,
                                 /*known_tab_node_ids=*/{},
                                 /*commit_cb=*/base::DoNothing());

  // First, we iterate over sync data to update our session_tracker_.
  if (!InitFromSyncModel(initial_sync_data, batch.sync_change_list())) {
    // The sync db didn't have a header node for us. Create one.
    auto specifics = std::make_unique<sync_pb::SessionSpecifics>();
    specifics->set_session_tag(current_machine_tag());
    sync_pb::SessionHeader* header_s = specifics->mutable_header();
    header_s->set_client_name(current_session_name_);
    header_s->set_device_type(local_device_info->device_type());
    batch.PutWithType(std::move(specifics), SyncChange::ACTION_ADD);
  }

#if defined(OS_ANDROID)
  std::string sync_machine_tag(BuildMachineTag(local_device_info->guid()));
  if (current_machine_tag().compare(sync_machine_tag) != 0)
    DeleteForeignSessionInternal(sync_machine_tag, batch.sync_change_list());
#endif

  merge_result.set_error(sync_processor_->ProcessSyncChanges(
      FROM_HERE, *batch.sync_change_list()));

  local_tab_pool_out_of_sync_ = false;

  // Check if anything has changed on the local client side.
  local_session_event_handler_ = std::make_unique<LocalSessionEventHandlerImpl>(
      /*delegate=*/this, sessions_client_, &session_tracker_);

  sessions_client_->GetLocalSessionEventRouter()->StartRoutingTo(
      local_session_event_handler_.get());
  return merge_result;
}

bool SessionsSyncManager::RebuildAssociations() {
  syncer::SyncDataList data(sync_processor_->GetAllSyncData(syncer::SESSIONS));
  std::unique_ptr<syncer::SyncErrorFactory> error_handler(
      std::move(error_handler_));
  std::unique_ptr<syncer::SyncChangeProcessor> processor(
      std::move(sync_processor_));

  StopSyncing(syncer::SESSIONS);
  syncer::SyncMergeResult merge_result = MergeDataAndStartSyncing(
      syncer::SESSIONS, data, std::move(processor), std::move(error_handler));
  return !merge_result.error().IsSet();
}

void SessionsSyncManager::StopSyncing(syncer::ModelType type) {
  sessions_client_->GetLocalSessionEventRouter()->Stop();
  local_session_event_handler_.reset();
  if (sync_processor_.get() && lost_navigations_recorder_.get()) {
    sync_processor_->RemoveLocalChangeObserver(
        lost_navigations_recorder_.get());
    lost_navigations_recorder_.reset();
  }
  sync_processor_.reset(nullptr);
  error_handler_.reset();
  session_tracker_.Clear();
  current_machine_tag_.clear();
  current_session_name_.clear();
}

syncer::SyncDataList SessionsSyncManager::GetAllSyncData(
    syncer::ModelType type) const {
  syncer::SyncDataList list;
  const SyncedSession* session = session_tracker_.LookupLocalSession();
  if (!session)
    return syncer::SyncDataList();

  // First construct the header node.
  sync_pb::EntitySpecifics header_entity;
  header_entity.mutable_session()->set_session_tag(current_machine_tag());
  sync_pb::SessionHeader* header_specifics =
      header_entity.mutable_session()->mutable_header();
  header_specifics->MergeFrom(session->ToSessionHeaderProto());
  syncer::SyncData data = syncer::SyncData::CreateLocalData(
      current_machine_tag(), current_session_name_, header_entity);
  list.push_back(data);

  for (const auto& win_iter : session->windows) {
    for (const auto& tab : win_iter.second->wrapped_window.tabs) {
      // TODO(zea): replace with with the correct tab node id once there's a
      // sync specific wrapper for SessionTab. This method is only used in
      // tests though, so it's fine for now. https://crbug.com/662597
      int tab_node_id = 0;
      sync_pb::EntitySpecifics entity;
      SessionTabToSpecifics(*tab, current_machine_tag(), tab_node_id)
          .Swap(entity.mutable_session());
      syncer::SyncData data = syncer::SyncData::CreateLocalData(
          TabNodeIdToTag(current_machine_tag(), tab_node_id),
          current_session_name_, entity);
      list.push_back(data);
    }
  }
  return list;
}

syncer::SyncError SessionsSyncManager::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!sync_processor_.get()) {
    syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                            "Models not yet associated.", syncer::SESSIONS);
    return error;
  }

  for (auto it = change_list.begin(); it != change_list.end(); ++it) {
    DCHECK(it->IsValid());
    DCHECK(it->sync_data().GetSpecifics().has_session());
    const sync_pb::SessionSpecifics& session =
        it->sync_data().GetSpecifics().session();
    const base::Time mtime =
        syncer::SyncDataRemote(it->sync_data()).GetModifiedTime();
    switch (it->change_type()) {
      case syncer::SyncChange::ACTION_DELETE:
        // Deletions are all or nothing (since we only ever delete entire
        // sessions). Therefore we don't care if it's a tab node or meta node,
        // and just ensure we've disassociated.
        if (current_machine_tag() == session.session_tag()) {
          // Another client has attempted to delete our local data (possibly by
          // error or a clock is inaccurate). Just ignore the deletion for now
          // to avoid any possible ping-pong delete/reassociate sequence, but
          // remember that this happened as our TabNodePool is inconsistent.
          local_tab_pool_out_of_sync_ = true;
          LOG(WARNING) << "Local session data deleted. Ignoring until next "
                       << "local navigation event.";
        } else if (session.has_header()) {
          // Disassociate only when header node is deleted. For tab node
          // deletions, the header node will be updated and foreign tab will
          // get deleted.
          DisassociateForeignSession(session.session_tag());
        } else if (session.has_tab()) {
          // The challenge here is that we don't know if this tab deletion is
          // being processed before or after the parent was updated to no longer
          // references the tab. Or, even more extreme, the parent has been
          // deleted as well. Tell the tracker to do what it can. The header's
          // update will mostly get us into the correct state, the only thing
          // this deletion needs to accomplish is make sure we never tell sync
          // to delete this tab later during garbage collection.
          session_tracker_.DeleteForeignTab(session.session_tag(),
                                            session.tab_node_id());
        }
        break;
      case syncer::SyncChange::ACTION_ADD:
      case syncer::SyncChange::ACTION_UPDATE:
        if (current_machine_tag() == session.session_tag()) {
          // We should only ever receive a change to our own machine's session
          // info if encryption was turned on. In that case, the data is still
          // the same, so we can ignore.
          LOG(WARNING) << "Dropping modification to local session.";
          return syncer::SyncError();
        }
        UpdateTrackerWithSpecifics(session, mtime, &session_tracker_);
        // If a favicon or favicon urls are present, load the URLs and visit
        // times into the in-memory favicon cache.
        if (session.has_tab()) {
          favicon_cache_.UpdateMappingsFromForeignTab(session.tab(), mtime);
        }
        break;
      default:
        NOTREACHED() << "Processing sync changes failed, unknown change type.";
    }
  }

  sessions_client_->NotifyForeignSessionUpdated();
  return syncer::SyncError();
}

syncer::SyncChange SessionsSyncManager::TombstoneTab(
    const sync_pb::SessionSpecifics& tab) {
  if (!tab.has_tab_node_id()) {
    LOG(WARNING) << "Old sessions node without tab node id; can't tombstone.";
    return syncer::SyncChange();
  } else {
    return syncer::SyncChange(
        FROM_HERE, SyncChange::ACTION_DELETE,
        SyncData::CreateLocalDelete(
            TabNodeIdToTag(current_machine_tag(), tab.tab_node_id()),
            syncer::SESSIONS));
  }
}

bool SessionsSyncManager::InitFromSyncModel(
    const syncer::SyncDataList& sync_data,
    syncer::SyncChangeList* new_changes) {
  bool found_current_header = false;
  int bad_foreign_hash_count = 0;
  for (auto it = sync_data.begin(); it != sync_data.end(); ++it) {
    const syncer::SyncData& data = *it;
    DCHECK(data.GetSpecifics().has_session());
    syncer::SyncDataRemote remote(data);
    const sync_pb::SessionSpecifics& specifics = data.GetSpecifics().session();
    if (specifics.session_tag().empty() ||
        (specifics.has_tab() &&
         (!specifics.has_tab_node_id() || !specifics.tab().has_tab_id()))) {
      syncer::SyncChange tombstone(TombstoneTab(specifics));
      if (tombstone.IsValid())
        new_changes->push_back(tombstone);
    } else if (specifics.session_tag() != current_machine_tag()) {
      if (TagHashFromSpecifics(specifics) == remote.GetClientTagHash()) {
        UpdateTrackerWithSpecifics(specifics, remote.GetModifiedTime(),
                                   &session_tracker_);
        // If a favicon or favicon urls are present, load the URLs and visit
        // times into the in-memory favicon cache.
        if (specifics.has_tab()) {
          favicon_cache_.UpdateMappingsFromForeignTab(specifics.tab(),
                                                      remote.GetModifiedTime());
        }
      } else {
        // In the past, like years ago, we believe that some session data was
        // created with bad tag hashes. This causes any change this client makes
        // to that foreign data (like deletion through garbage collection) to
        // trigger a data type error because the tag looking mechanism fails. So
        // look for these and delete via remote SyncData, which uses a server id
        // lookup mechanism instead, see https://crbug.com/604657.
        bad_foreign_hash_count++;
        new_changes->push_back(
            syncer::SyncChange(FROM_HERE, SyncChange::ACTION_DELETE, remote));
      }
    } else {
      // This is previously stored local session information.
      if (specifics.has_header() && !found_current_header) {
        // This is our previous header node, reuse it.
        found_current_header = true;

        UpdateTrackerWithSpecifics(specifics, remote.GetModifiedTime(),
                                   &session_tracker_);
        DVLOG(1) << "Loaded local header.";
      } else {
        if (specifics.has_header() || !specifics.has_tab()) {
          LOG(WARNING) << "Found more than one session header node with local "
                       << "tag.";
          syncer::SyncChange tombstone(TombstoneTab(specifics));
          if (tombstone.IsValid())
            new_changes->push_back(tombstone);
        } else if (specifics.tab().tab_id() <= 0) {
          LOG(WARNING) << "Found tab node with invalid tab id.";
          syncer::SyncChange tombstone(TombstoneTab(specifics));
          if (tombstone.IsValid())
            new_changes->push_back(tombstone);
        } else {
          // This is a valid old tab node, add it to the tracker and associate
          // it (using the new tab id).
          DVLOG(1) << "Associating local tab " << specifics.tab().tab_id()
                   << " with node " << specifics.tab_node_id();

          session_tracker_.ReassociateLocalTab(
              specifics.tab_node_id(),
              SessionID::FromSerializedValue(specifics.tab().tab_id()));
          UpdateTrackerWithSpecifics(specifics, remote.GetModifiedTime(),
                                     &session_tracker_);
        }
      }
    }
  }

  // Cleanup all foreign sessions, since orphaned tabs may have been added after
  // the header.
  for (const auto* session :
       session_tracker_.LookupAllForeignSessions(SyncedSessionTracker::RAW)) {
    session_tracker_.CleanupSession(session->session_tag);
  }

  UMA_HISTOGRAM_COUNTS_100("Sync.SessionsBadForeignHashOnMergeCount",
                           bad_foreign_hash_count);

  return found_current_header;
}

void SessionsSyncManager::InitializeCurrentMachineTag(
    const std::string& cache_guid) {
  DCHECK(current_machine_tag_.empty());
  std::string persisted_guid;
  persisted_guid =
      sessions_client_->GetSessionSyncPrefs()->GetSyncSessionsGUID();
  if (!persisted_guid.empty()) {
    current_machine_tag_ = persisted_guid;
    DVLOG(1) << "Restoring persisted session sync guid: " << persisted_guid;
  } else {
    DCHECK(!cache_guid.empty());
    current_machine_tag_ = BuildMachineTag(cache_guid);
    DVLOG(1) << "Creating session sync guid: " << current_machine_tag_;
    sessions_client_->GetSessionSyncPrefs()->SetSyncSessionsGUID(
        current_machine_tag_);
  }
}

void SessionsSyncManager::DeleteForeignSessionInternal(
    const std::string& tag,
    syncer::SyncChangeList* change_output) {
  if (tag == current_machine_tag()) {
    LOG(ERROR) << "Attempting to delete local session. This is not currently "
               << "supported.";
    return;
  }

  const std::set<int> tab_node_ids_to_delete =
      session_tracker_.LookupTabNodeIds(tag);
  if (DisassociateForeignSession(tag)) {
    // Only tell sync to delete the header if there was one.
    change_output->push_back(
        syncer::SyncChange(FROM_HERE, SyncChange::ACTION_DELETE,
                           SyncData::CreateLocalDelete(tag, syncer::SESSIONS)));
  }
  AppendDeletionsForTabNodes(tab_node_ids_to_delete, tag, change_output);

  sessions_client_->NotifyForeignSessionUpdated();
}

void SessionsSyncManager::DeleteForeignSessionFromUI(const std::string& tag) {
  syncer::SyncChangeList changes;
  DeleteForeignSessionInternal(tag, &changes);
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

bool SessionsSyncManager::DisassociateForeignSession(
    const std::string& foreign_session_tag) {
  DCHECK_NE(foreign_session_tag, current_machine_tag());
  DVLOG(1) << "Disassociating session " << foreign_session_tag;
  return session_tracker_.DeleteForeignSession(foreign_session_tag);
}

std::unique_ptr<LocalSessionEventHandlerImpl::WriteBatch>
SessionsSyncManager::CreateLocalSessionWriteBatch() {
  return std::make_unique<SyncChangeListWriteBatch>(
      current_machine_tag(), current_session_name_,
      /*known_tab_node_ids=*/
      session_tracker_.LookupTabNodeIds(current_machine_tag()),
      /*commit_cb=*/
      base::BindOnce(&SessionsSyncManager::ProcessLocalSessionSyncChanges,
                     base::AsWeakPtr(this)));
}

void SessionsSyncManager::TrackLocalNavigationId(base::Time timestamp,
                                                 int unique_id) {
  global_id_mapper_.TrackNavigationId(timestamp, unique_id);
}

void SessionsSyncManager::OnPageFaviconUpdated(const GURL& page_url) {
  favicon_cache_.OnPageFaviconUpdated(page_url, base::Time::Now());
}

void SessionsSyncManager::OnFaviconVisited(const GURL& page_url,
                                           const GURL& favicon_url) {
  favicon_cache_.OnFaviconVisited(page_url, favicon_url);
}

FaviconCache* SessionsSyncManager::GetFaviconCache() {
  return &favicon_cache_;
}

SessionsGlobalIdMapper* SessionsSyncManager::GetGlobalIdMapper() {
  return &global_id_mapper_;
}

OpenTabsUIDelegate* SessionsSyncManager::GetOpenTabsUIDelegate() {
  return &open_tabs_ui_delegate_;
}

void SessionsSyncManager::DoGarbageCollection() {
  // Iterate through all the sessions and delete any with age older than
  // |stale_session_threshold_days_|.
  syncer::SyncChangeList changes;
  for (const auto* session :
       session_tracker_.LookupAllForeignSessions(SyncedSessionTracker::RAW)) {
    int session_age_in_days =
        (base::Time::Now() - session->modified_time).InDays();
    if (session_age_in_days > stale_session_threshold_days_) {
      std::string session_tag = session->session_tag;
      DVLOG(1) << "Found stale session " << session_tag << " with age "
               << session_age_in_days << ", deleting.";
      DeleteForeignSessionInternal(session_tag, &changes);
    }
  }

  if (!changes.empty())
    sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

// static
std::string SessionsSyncManager::TagHashFromSpecifics(
    const sync_pb::SessionSpecifics& specifics) {
  return syncer::GenerateSyncableHash(syncer::SESSIONS,
                                      TagFromSpecifics(specifics));
}

void SessionsSyncManager::ProcessLocalSessionSyncChanges(
    const syncer::SyncChangeList& change_list) {
  if (local_tab_pool_out_of_sync_) {
    // If our tab pool is corrupt, pay the price of a full re-association to
    // fix things up.  This takes care of the new tab modification as well.
    bool rebuild_association_succeeded = RebuildAssociations();
    DCHECK(!rebuild_association_succeeded || !local_tab_pool_out_of_sync_);
    return;
  }

  sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
}

}  // namespace sync_sessions
