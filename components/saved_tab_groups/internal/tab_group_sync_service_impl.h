// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_SERVICE_IMPL_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "components/data_sharing/public/personal_collaboration_data/personal_collaboration_data_service.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/internal/shared_tab_group_account_data_sync_bridge.h"
#include "components/saved_tab_groups/internal/shared_tab_group_data_sync_bridge.h"
#include "components/saved_tab_groups/internal/tab_group_sync_bridge_mediator.h"
#include "components/saved_tab_groups/internal/tab_group_sync_coordinator.h"
#include "components/saved_tab_groups/internal/tab_group_sync_personal_collaboration_data_handler.h"
#include "components/saved_tab_groups/public/collaboration_finder.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/versioning_message_controller.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/collaboration_id.h"

class PrefService;

namespace data_sharing {
class Logger;
}  // namespace data_sharing

namespace tab_groups {

struct SyncDataTypeConfiguration;

// The internal implementation of the TabGroupSyncService.
class TabGroupSyncServiceImpl : public TabGroupSyncService,
                                public SavedTabGroupModelObserver,
                                public CollaborationFinder::Client,
                                public signin::IdentityManager::Observer {
 public:
  // `saved_tab_group_configuration` must not be `nullptr`.
  // `shared_tab_group_configuration` should be provided if feature is enabled.
  TabGroupSyncServiceImpl(
      std::unique_ptr<SavedTabGroupModel> model,
      std::unique_ptr<SyncDataTypeConfiguration> saved_tab_group_configuration,
      std::unique_ptr<SyncDataTypeConfiguration> shared_tab_group_configuration,
      std::unique_ptr<SyncDataTypeConfiguration>
          shared_tab_group_account_configuration,
      PrefService* pref_service,
      std::unique_ptr<TabGroupSyncMetricsLogger> metrics_logger,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      signin::IdentityManager* identity_manager,
      data_sharing::personal_collaboration_data::
          PersonalCollaborationDataService* personal_collaboration_data_service,
      std::unique_ptr<CollaborationFinder> collaboration_finder,
      data_sharing::Logger* logger);
  ~TabGroupSyncServiceImpl() override;

  // Disallow copy/assign.
  TabGroupSyncServiceImpl(const TabGroupSyncServiceImpl&) = delete;
  TabGroupSyncServiceImpl& operator=(const TabGroupSyncServiceImpl&) = delete;

  // TabGroupSyncService implementation.
  void SetTabGroupSyncDelegate(
      std::unique_ptr<TabGroupSyncDelegate> delegate) override;
  void AddGroup(SavedTabGroup group) override;
  void RemoveGroup(const LocalTabGroupID& local_id) override;
  void RemoveGroup(const base::Uuid& sync_id) override;
  void UpdateVisualData(
      const LocalTabGroupID local_group_id,
      const tab_groups::TabGroupVisualData* visual_data) override;
  void UpdateGroupPosition(const base::Uuid& sync_id,
                           std::optional<bool> is_pinned,
                           std::optional<int> new_index) override;

  void UpdateBookmarkNodeId(
      const base::Uuid& sync_id,
      std::optional<base::Uuid> bookmark_node_id) override;

  void AddTab(const LocalTabGroupID& group_id,
              const LocalTabID& tab_id,
              const std::u16string& title,
              const GURL& url,
              std::optional<size_t> position) override;

  void AddUrl(const base::Uuid& group_id,
              const std::u16string& title,
              const GURL& url) override;

  void NavigateTab(const LocalTabGroupID& group_id,
                   const LocalTabID& tab_id,
                   const GURL& url,
                   const std::u16string& title) override;
  void UpdateTabProperties(const LocalTabGroupID& group_id,
                           const LocalTabID& tab_id,
                           const SavedTabGroupTabBuilder& tab_builder) override;
  void RemoveTab(const LocalTabGroupID& group_id,
                 const LocalTabID& tab_id) override;
  void MoveTab(const LocalTabGroupID& group_id,
               const LocalTabID& tab_id,
               int new_group_index) override;
  void OnTabSelected(const std::optional<LocalTabGroupID>& group_id,
                     const LocalTabID& tab_id,
                     const std::u16string& title) override;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void SaveGroup(SavedTabGroup group) override;
  void UnsaveGroup(const LocalTabGroupID& local_id) override;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  void MakeTabGroupShared(const LocalTabGroupID& local_group_id,
                          const syncer::CollaborationId& collaboration_id,
                          TabGroupSharingCallback callback) override;
  void MakeTabGroupSharedForTesting(
      const LocalTabGroupID& local_group_id,
      const syncer::CollaborationId& collaboration_id) override;
  void MakeTabGroupUnsharedForTesting(
      const LocalTabGroupID& local_group_id) override;

  void AboutToUnShareTabGroup(const LocalTabGroupID& local_group_id,
                              base::OnceClosure on_complete_callback) override;
  void OnTabGroupUnShareComplete(const LocalTabGroupID& local_group_id,
                                 bool success) override;
  void OnCollaborationRemoved(
      const syncer::CollaborationId& collaboration_id) override;

  std::vector<const SavedTabGroup*> ReadAllGroups() const override;
  std::vector<SavedTabGroup> GetAllGroups() const override;
  std::optional<SavedTabGroup> GetGroup(const base::Uuid& guid) const override;
  std::optional<SavedTabGroup> GetGroup(
      const LocalTabGroupID& local_id) const override;
  std::optional<SavedTabGroup> GetGroup(
      const EitherGroupID& either_id) const override;
  std::vector<LocalTabGroupID> GetDeletedGroupIds() const override;
  std::optional<std::u16string> GetTitleForPreviouslyExistingSharedTabGroup(
      const syncer::CollaborationId& collaboration_id) const override;

  std::optional<LocalTabGroupID> OpenTabGroup(
      const base::Uuid& sync_group_id,
      std::unique_ptr<TabGroupActionContext> context) override;
  void UpdateLocalTabGroupMapping(const base::Uuid& sync_id,
                                  const LocalTabGroupID& local_id,
                                  OpeningSource opening_source) override;
  void RemoveLocalTabGroupMapping(const LocalTabGroupID& local_id,
                                  ClosingSource closing_source) override;
  void UpdateLocalTabId(const LocalTabGroupID& local_group_id,
                        const base::Uuid& sync_tab_id,
                        const LocalTabID& local_tab_id) override;
  void ConnectLocalTabGroup(const base::Uuid& sync_id,
                            const LocalTabGroupID& local_id,
                            OpeningSource opening_source) override;

  bool IsRemoteDevice(
      const std::optional<std::string>& cache_guid) const override;

  bool WasTabGroupClosedLocally(
      const base::Uuid& sync_tab_group_id) const override;

  std::u16string GetTabTitle(const LocalTabID& local_tab_id) override;
  std::set<LocalTabID> GetSelectedTabs() override;

  void RecordTabGroupEvent(const EventDetails& event_details) override;

  void UpdateArchivalStatus(const base::Uuid& sync_id,
                            bool archival_status) override;

  void UpdateTabLastSeenTime(const base::Uuid& group_id,
                             const base::Uuid& tab_id,
                             TriggerSource source) override;

  TabGroupSyncMetricsLogger* GetTabGroupSyncMetricsLogger() override;

  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSavedTabGroupControllerDelegate() override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSharedTabGroupControllerDelegate() override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSharedTabGroupAccountControllerDelegate() override;

  std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser() override;
  void GetURLRestriction(
      const GURL& url,
      TabGroupSyncService::UrlRestrictionCallback callback) override;

  std::unique_ptr<std::vector<SavedTabGroup>>
  TakeSharedTabGroupsAvailableAtStartupForMessaging() override;
  bool HadSharedTabGroupsLastSession(bool open_shared_tab_groups) override;
  VersioningMessageController* GetVersioningMessageController() override;
  void OnLastTabClosed(const SavedTabGroup& saved_tab_group) override;

  void AddObserver(TabGroupSyncService::Observer* observer) override;
  void RemoveObserver(TabGroupSyncService::Observer* observer) override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // tab_groups::CollaborationFinder::Client:
  void OnCollaborationAvailable(
      const syncer::CollaborationId& collaboration_id) override;

  // For testing only.
  void SetIsInitializedForTesting(bool initialized) override;
  CollaborationFinder* GetCollaborationFinderForTesting() override;

  // Called to set a coordinator that will manage all interactions with the tab
  // model UI layer.
  void SetCoordinator(std::unique_ptr<TabGroupSyncCoordinator> coordinator);

  SavedTabGroupModel* GetModel() { return model_.get(); }

 private:
  struct TabGroupSharingTimeoutInfo {
    TabGroupSharingTimeoutInfo();
    ~TabGroupSharingTimeoutInfo();

    base::OneShotTimer timer;
    TabGroupSharingCallback callback;
  };

  // KeyedService:
  void Shutdown() override;

  // SavedTabGroupModelObserver implementation.
  void SavedTabGroupReorderedLocally() override;
  void SavedTabGroupReorderedFromSync() override;
  void SavedTabGroupAddedFromSync(const base::Uuid& guid) override;
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;
  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;
  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup& removed_group) override;
  void SavedTabGroupRemovedLocally(const SavedTabGroup& removed_group) override;
  void SavedTabGroupLocalIdChanged(const base::Uuid& saved_group_id) override;
  void SavedTabGroupTabLastSeenTimeUpdated(const base::Uuid& tab_id,
                                           TriggerSource source) override;
  void SavedTabGroupModelLoaded() override;
  void OnSyncBridgeUpdateTypeChanged(
      SyncBridgeUpdateType sync_bridge_update_type) override;
  void TabGroupTransitioningToSavedRemovedFromSync(
      const base::Uuid& saved_group_id) override;

  // Called to notify the observers that service initialization is complete.
  void NotifyServiceInitialized();

  // Consolidation methods for adapting to observer signals from either
  // direction (local -> remote or remote -> local).
  // TODO(shaktisahu): Make SavedTabGroupModelObserver consolidate these signals
  // directly at some point.
  void HandleTabGroupAdded(const base::Uuid& guid, TriggerSource source);
  void HandleTabGroupUpdated(const base::Uuid& group_guid,
                             const std::optional<base::Uuid>& tab_guid,
                             TriggerSource source);
  void NotifyTabGroupAdded(const base::Uuid& guid, TriggerSource source);
  void NotifyTabGroupUpdated(const base::Uuid& guid, TriggerSource source);
  void NotifyTabGroupMigrated(const base::Uuid& new_group_guid,
                              TriggerSource source);

  void HandleTabGroupRemoved(const SavedTabGroup& removed_group,
                             TriggerSource source);
  void HandleTabGroupsReordered(TriggerSource source);

  void NotifyOnSyncBridgeUpdateTypeChanged(
      SyncBridgeUpdateType sync_bridge_update_type);

  // Read and write deleted local group IDs to disk. We add a local ID in
  // response to a group deletion event from sync. We clear that ID only when
  // RemoveLocalTabGroupMapping is invoked from the UI.
  // On startup, UI invokes GetDeletedGroupIdsFromPref to clean up any deleted
  // groups from tab model.
  std::vector<LocalTabGroupID> GetDeletedGroupIdsFromPref() const;
  void AddDeletedGroupIdToPref(const LocalTabGroupID& local_id,
                               const base::Uuid& sync_id);
  void RemoveDeletedGroupIdFromPref(const LocalTabGroupID& local_id);

  void AddLocallyClosedGroupIdToPref(const base::Uuid& sync_id);
  void RemoveLocallyClosedGroupIdFromPref(const base::Uuid& sync_id);

  // Wrapper function that calls all metric recording functions.
  void RecordMetrics();

  // Force remove all tab groups that are currently not open in the local tab
  // model. This is used on a certain finch group in order to fix an earlier
  // bug.
  void ForceRemoveClosedTabGroupsOnStartup();

  // Called to clean up originating saved tab groups if needed.
  void CleanUpOriginatingSavedTabGroupsIfNeeded();

  // Helper function to update attributions for a group and optionally a tab.
  void UpdateAttributions(
      const LocalTabGroupID& group_id,
      const std::optional<LocalTabID>& tab_id = std::nullopt);

  // Helper method to update shared attributions for a group or tab. When
  // `tab_id` is provided, the shared attributions are updated for the tab only.
  // This method updates the shared attributions while UpdateAttributions()
  // updates attributions for the saved tab groups.
  void UpdateSharedAttributions(
      const LocalTabGroupID& group_id,
      const std::optional<LocalTabID>& tab_id = std::nullopt);

  // Helper function to log a tab group event in histograms.
  void LogEvent(TabGroupEvent event,
                LocalTabGroupID group_id,
                const std::optional<LocalTabID>& tab_id = std::nullopt);

  // Creates a copy of all shared tab groups from the model and stores them in
  // `shared_tab_groups_available_at_startup_for_messaging_` for later
  // retrieval.
  void StoreSharedTabGroupsAvailableAtStartupForMessaging();

  // Transitions the originating saved tab group to the given shared tab group
  // if the saved tab group is open in the tab strip. Returns true if the group
  // was transitioned.
  bool TransitionSavedToSharedTabGroupIfNeeded(
      const SavedTabGroup& shared_group);

  // Transitions the originating shared tab group to the given saved tab group.
  // Returns true if the group is transitioned.
  bool TransitionSharedToSavedTabGroupIfNeeded(
      const SavedTabGroup& saved_group);

  // Transitions a originating tab group to a new tab group. Called when
  // either a saved tab group is becoming shared, or when a shared tab group is
  // becoming private. This call will find the orignating tab group from the
  // `tab_group`'s originating group guid, and replace it with `tab_group`.
  // `opening_source` is the reason for adding the new group, and
  // `closing_source` is the reason for removing the originating tab group.
  // Returns true if the group is transitioned.
  bool TransitionOriginatingTabGroupToNewGroupIfNeeded(
      const SavedTabGroup& tab_group,
      OpeningSource opening_source,
      ClosingSource closing_source);

  // Helper method for implementation shared by AddUrl and AddTab.
  void AddTabInternal(SavedTabGroupTab tab, const SavedTabGroup* group);

  // Helper method called by NavigateTab() when UrlRestriction is retrieved.
  void NavigateTabInternal(
      const LocalTabGroupID& group_id,
      const LocalTabID& tab_id,
      const GURL& url,
      const std::u16string& title,
      const GURL& previous_tab_url,
      const std::optional<proto::UrlRestriction>& url_restriction);

  void NotifyTabSelected();

  void OnTabGroupSharingTimeout(const base::Uuid& group_guid);

  void NotifyTabGroupSharingResult(const base::Uuid& group_guid,
                                   TabGroupSharingResult result);

  // Whether a given group should show up in TabGroups
  bool ShouldExposeSavedTabGroupInList(const SavedTabGroup& group) const;

  // Find tab group by collaboration Id.
  std::optional<SavedTabGroup> FindGroupWithCollaborationId(
      const syncer::CollaborationId& collaboration_id);

  // Updates the last seen time for any focused tab. Invoked for remote updates.
  // This is because if a tab is updated from remote while being focused, it
  // should automatically be marked as seen.
  void UpdateLastSeenTimeForAnyFocusedTabForRemoteUpdates(
      const SavedTabGroup* group,
      TriggerSource source);

  // Check if there are any groups that are stuck in the middle of transitioning
  // to shared and completes the transition.
  void FinishTransitionToSharedIfNotCompleted();

  // Register PageEntities optimization type if there is a shared tab group.
  void RegisterPageEntityOptimizationTypeIfNeeded();

  THREAD_CHECKER(thread_checker_);

  // The in-memory model representing the currently present saved tab groups.
  std::unique_ptr<SavedTabGroupModel> model_;

  // Sync bridges and data storage for both saved and shared tab group data.
  std::unique_ptr<TabGroupSyncBridgeMediator> sync_bridge_mediator_;

  // Sync bridge for shared tab group account data.
  std::unique_ptr<SharedTabGroupAccountDataSyncBridge>
      shared_tab_group_account_data_bridge_;

  // The UI coordinator to apply changes between local tab groups and the
  // TabGroupSyncService.
  std::unique_ptr<TabGroupSyncCoordinator> coordinator_;

  // Helper class for logging metrics.
  std::unique_ptr<TabGroupSyncMetricsLogger> metrics_logger_;

  // For finding collaboration availability info from DataSharingService.
  std::unique_ptr<CollaborationFinder> collaboration_finder_;

  // Logger for logging to debug UI.
  raw_ptr<data_sharing::Logger> logger_ = nullptr;

  // The pref service for storing migration status.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Handles personal collaboration data updates.
  std::unique_ptr<TabGroupSyncPersonalCollaborationDataHandler>
      personal_collaboration_data_handler_;

  // Whether the initialization has been completed, i.e. all the groups and the
  // ID mappings have been loaded into memory.
  bool is_initialized_ = false;

  // Groups with zero tabs are groups that still haven't received their tabs
  // from sync. UI can't handle these groups, hence the service needs to wait
  // before notifying the observers.
  std::set<base::Uuid> empty_groups_;

  // Keeps track of shared tab groups that are waiting for their respective
  // people groups to be available in DataSharingService backend. UI can't
  // handle these groups, hence the service needs to wait before notifying the
  // observers. Once the group becomes available, OnTabGroupAdded() will be
  // invoked for the shared tab group.
  std::vector<std::tuple<syncer::CollaborationId, base::Uuid, TriggerSource>>
      shared_tab_groups_waiting_for_collaboration_;

  // Obsevers of the model.
  base::ObserverList<TabGroupSyncService::Observer> observers_;

  // Temporary storage for shared tab groups that were available at startup,
  // before applying local changes. This is retrieved by the
  // MessagingBackendService when it starts up to safely know what changes are
  // new, and what were there from before, without needing to persist any data
  // on its own, but still be able to calculate deltas of all network triggered
  // updates.
  std::unique_ptr<std::vector<SavedTabGroup>>
      shared_tab_groups_available_at_startup_for_messaging_;

  // Whether shared tab groups existed during startup.
  bool had_shared_tab_groups_on_startup_ = false;

  // Whether open shared tab groups existed during startup.
  bool had_open_shared_tab_groups_on_startup_ = false;

  // Temporary in-memory mapping from collaboration ID to title for tab groups
  // that we/ have previously known about. This is to facilitate displaying of
  // tab group titles in the UI when a user is removed from a tab group.
  std::unordered_map<syncer::CollaborationId, std::u16string>
      titles_for_previously_existing_shared_tab_groups_;

  // Keeps track of API calls received before the service is initialized.
  // Once the initialization is complete, these callbacks are run in the order
  // they were received. External observers are notified of init completion only
  // after this step. Currently used only for UpdateLocalTabGroupMapping()
  // calls.
  base::circular_deque<base::OnceClosure> pending_actions_;

  // Keeps track of API calls received when not signed in. Unlike most other
  // methods of this class, these API calls cannot proceed without a signed in
  // account (e.g. they need user attribution). Currently required
  // for MakeTabGroupShared() only.
  // Once the sign-in and initial sync is complete for the account, these
  // callbacks will be run in the order they were received.
  base::circular_deque<base::OnceClosure> pending_actions_waiting_sign_in_;

  // A handle to optimization guide for information about synced URLs.
  raw_ptr<optimization_guide::OptimizationGuideDecider> opt_guide_ = nullptr;

  // Keeps track of the tab group sharing progress. There can be multiple
  // sharing requests for different tab groups (e.g. from different windows).
  std::map<base::Uuid, TabGroupSharingTimeoutInfo>
      tab_group_sharing_timeout_info_;

  // The versioning message controller which is responsible for business logic
  // related to shared tab groups versioning related messages.
  std::unique_ptr<VersioningMessageController> versioning_message_controller_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  bool page_entity_optimization_type_registered_ = false;

  base::WeakPtrFactory<TabGroupSyncServiceImpl> weak_ptr_factory_{this};
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_SERVICE_IMPL_H_
