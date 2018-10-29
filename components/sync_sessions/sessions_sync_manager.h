// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSIONS_SYNC_MANAGER_H_
#define COMPONENTS_SYNC_SESSIONS_SESSIONS_SYNC_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync_sessions/abstract_sessions_sync_manager.h"
#include "components/sync_sessions/favicon_cache.h"
#include "components/sync_sessions/local_session_event_handler_impl.h"
#include "components/sync_sessions/lost_navigations_recorder.h"
#include "components/sync_sessions/open_tabs_ui_delegate_impl.h"
#include "components/sync_sessions/sessions_global_id_mapper.h"
#include "components/sync_sessions/synced_session.h"
#include "components/sync_sessions/synced_session_tracker.h"

namespace syncer {
class SyncErrorFactory;
}  // namespace syncer

namespace sync_pb {
class SessionSpecifics;
class SessionTab;
}  // namespace sync_pb

namespace extensions {
class ExtensionSessionsTest;
}  // namespace extensions

namespace sync_sessions {

// Contains all logic for associating the Chrome sessions model and
// the sync sessions model.
class SessionsSyncManager : public AbstractSessionsSyncManager,
                            public syncer::SyncableService,
                            public LocalSessionEventHandlerImpl::Delegate {
 public:
  explicit SessionsSyncManager(SyncSessionsClient* sessions_client);
  ~SessionsSyncManager() override;

  // AbstractSessionsSyncManager implementation.
  void ScheduleGarbageCollection() override;
  FaviconCache* GetFaviconCache() override;
  SessionsGlobalIdMapper* GetGlobalIdMapper() override;
  OpenTabsUIDelegate* GetOpenTabsUIDelegate() override;
  syncer::SyncableService* GetSyncableService() override;
  syncer::ModelTypeSyncBridge* GetModelTypeSyncBridge() override;

  // syncer::SyncableService implementation.
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  // LocalSessionEventHandlerImpl::Delegate implementation.
  std::unique_ptr<LocalSessionEventHandlerImpl::WriteBatch>
  CreateLocalSessionWriteBatch() override;
  void TrackLocalNavigationId(base::Time timestamp, int unique_id) override;
  void OnPageFaviconUpdated(const GURL& page_url) override;
  void OnFaviconVisited(const GURL& page_url, const GURL& favicon_url) override;

  // Returns the tag used to uniquely identify this machine's session in the
  // sync model.
  const std::string& current_machine_tag() const {
    DCHECK(!current_machine_tag_.empty());
    return current_machine_tag_;
  }

  const std::string GetCurrentSessionNameForTest() const {
    return current_session_name_;
  }

  // Triggers garbage collection of stale sessions (as defined by
  // |stale_session_threshold_days_|). This is called every time we see new
  // sessions data downloaded (sync cycles complete).
  void DoGarbageCollection();

 private:
  friend class extensions::ExtensionSessionsTest;
  friend class SessionsSyncManagerTest;
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, BlockedNavigations);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, DeleteForeignSession);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           ProcessForeignDeleteTabsWithShadowing);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           ProcessForeignDeleteTabsWithReusedNodeIds);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, MergeDeletesBadHash);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           MergeLocalSessionExistingTabs);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           AssociateWindowsDontReloadTabs);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, SwappedOutOnRestore);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           ProcessRemoteDeleteOfLocalSession);

  void InitializeCurrentMachineTag(const std::string& cache_guid);

  // Returns true if |sync_data| contained a header node for the current
  // machine, false otherwise. |new_changes| is a link to the SyncChange
  // pipeline that exists in the caller's context. This function will append
  // necessary changes for processing later.
  bool InitFromSyncModel(const syncer::SyncDataList& sync_data,
                         syncer::SyncChangeList* new_changes);

  // Helper to construct a deletion SyncChange for a *tab node*.
  // Caller should check IsValid() on the returned change, as it's possible
  // this node could not be deleted.
  syncer::SyncChange TombstoneTab(const sync_pb::SessionSpecifics& tab);

  // Removes a foreign session from our internal bookkeeping.
  // Returns true if the session was found and deleted, false if no data was
  // found for that session.  This will *NOT* trigger sync deletions. See
  // DeleteForeignSession below.
  bool DisassociateForeignSession(const std::string& foreign_session_tag);

  // Delete a foreign session and all its sync data.
  // |change_output| *must* be provided as a link to the SyncChange pipeline
  // that exists in the caller's context. This function will append necessary
  // changes for processing later.
  void DeleteForeignSessionInternal(const std::string& tag,
                                    syncer::SyncChangeList* change_output);

  // Same as above but it also notifies the processor.
  void DeleteForeignSessionFromUI(const std::string& tag);

  // Stops and re-starts syncing to rebuild association mappings. Returns true
  // when re-starting succeeds.
  // See |local_tab_pool_out_of_sync_|.
  bool RebuildAssociations();

  // Calculates the tag hash from a specifics object. Calculating the hash is
  // something we typically want to avoid doing in the model type like this.
  // However, the only place that understands how to generate a tag from the
  // specifics is the model type, ie us. We need to generate the tag because it
  // is not passed over the wire for remote data. The use case this function was
  // created for is detecting bad tag hashes from remote data, see
  // https://crbug.com/604657.
  static std::string TagHashFromSpecifics(
      const sync_pb::SessionSpecifics& specifics);

  void ProcessLocalSessionSyncChanges(
      const syncer::SyncChangeList& change_list);

  // The client of this sync sessions datatype.
  SyncSessionsClient* const sessions_client_;

  SyncedSessionTracker session_tracker_;
  SessionsGlobalIdMapper global_id_mapper_;
  FaviconCache favicon_cache_;
  OpenTabsUIDelegateImpl open_tabs_ui_delegate_;

  // Instantiated when sync is enabled.
  std::unique_ptr<LocalSessionEventHandlerImpl> local_session_event_handler_;

  // Tracks whether our local representation of which sync nodes map to what
  // tabs (belonging to the current local session) is inconsistent.  This can
  // happen if a foreign client deems our session as "stale" and decides to
  // delete it. Rather than respond by bullishly re-creating our nodes
  // immediately, which could lead to ping-pong sequences, we give the benefit
  // of the doubt and hold off until another local navigation occurs, which
  // proves that we are still relevant.
  bool local_tab_pool_out_of_sync_;

  std::unique_ptr<syncer::SyncErrorFactory> error_handler_;
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Unique client tag.
  std::string current_machine_tag_;

  // User-visible machine name to populate header.
  std::string current_session_name_;

  // Number of days without activity after which we consider a session to be
  // stale and a candidate for garbage collection.
  int stale_session_threshold_days_;

  std::unique_ptr<sync_sessions::LostNavigationsRecorder>
      lost_navigations_recorder_;

  DISALLOW_COPY_AND_ASSIGN(SessionsSyncManager);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSIONS_SYNC_MANAGER_H_
