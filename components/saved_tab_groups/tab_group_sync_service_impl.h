// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_IMPL_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/shared_tab_group_data_sync_bridge.h"
#include "components/saved_tab_groups/tab_group_sync_bridge_mediator.h"
#include "components/saved_tab_groups/tab_group_sync_coordinator.h"
#include "components/saved_tab_groups/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "components/saved_tab_groups/types.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/sync_data.h"

class PrefService;

namespace tab_groups {

struct SyncDataTypeConfiguration;

// The internal implementation of the TabGroupSyncService.
class TabGroupSyncServiceImpl : public TabGroupSyncService,
                                public SavedTabGroupModelObserver {
 public:
  // `saved_tab_group_configuration` must not be `nullptr`.
  // `shared_tab_group_configuration` should be provided if feature is enabled.
  TabGroupSyncServiceImpl(
      std::unique_ptr<SavedTabGroupModel> model,
      std::unique_ptr<SyncDataTypeConfiguration> saved_tab_group_configuration,
      std::unique_ptr<SyncDataTypeConfiguration> shared_tab_group_configuration,
      PrefService* pref_service,
      std::unique_ptr<TabGroupSyncMetricsLogger> metrics_logger);
  ~TabGroupSyncServiceImpl() override;

  // Disallow copy/assign.
  TabGroupSyncServiceImpl(const TabGroupSyncServiceImpl&) = delete;
  TabGroupSyncServiceImpl& operator=(const TabGroupSyncServiceImpl&) = delete;

  // Called to set a coordinator that will manage all interactions with the tab
  // model UI layer.
  void SetCoordinator(std::unique_ptr<TabGroupSyncCoordinator> coordinator);

  // TabGroupSyncService implementation.
  void AddGroup(SavedTabGroup group) override;
  void RemoveGroup(const LocalTabGroupID& local_id) override;
  void RemoveGroup(const base::Uuid& sync_id) override;
  void UpdateVisualData(
      const LocalTabGroupID local_group_id,
      const tab_groups::TabGroupVisualData* visual_data) override;
  void UpdateGroupPosition(const base::Uuid& sync_id,
                           std::optional<bool> is_pinned,
                           std::optional<int> new_index) override;

  void AddTab(const LocalTabGroupID& group_id,
              const LocalTabID& tab_id,
              const std::u16string& title,
              GURL url,
              std::optional<size_t> position) override;
  void UpdateTab(const LocalTabGroupID& group_id,
                 const LocalTabID& tab_id,
                 const SavedTabGroupTabBuilder& tab_builder) override;
  void RemoveTab(const LocalTabGroupID& group_id,
                 const LocalTabID& tab_id) override;
  void MoveTab(const LocalTabGroupID& group_id,
               const LocalTabID& tab_id,
               int new_group_index) override;
  void OnTabSelected(const LocalTabGroupID& group_id,
                     const LocalTabID& tab_id) override;

  void MakeTabGroupShared(const LocalTabGroupID& local_group_id,
                          std::string_view collaboration_id) override;

  std::vector<SavedTabGroup> GetAllGroups() override;
  std::optional<SavedTabGroup> GetGroup(const base::Uuid& guid) override;
  std::optional<SavedTabGroup> GetGroup(
      const LocalTabGroupID& local_id) override;
  std::vector<LocalTabGroupID> GetDeletedGroupIds() override;
  void OpenTabGroup(const base::Uuid& sync_group_id,
                    std::unique_ptr<TabGroupActionContext> context) override;
  void UpdateLocalTabGroupMapping(const base::Uuid& sync_id,
                                  const LocalTabGroupID& local_id) override;
  void RemoveLocalTabGroupMapping(const LocalTabGroupID& local_id) override;
  void UpdateLocalTabId(const LocalTabGroupID& local_group_id,
                        const base::Uuid& sync_tab_id,
                        const LocalTabID& local_tab_id) override;
  void ConnectLocalTabGroup(const base::Uuid& sync_id,
                            const LocalTabGroupID& local_id) override;

  bool IsRemoteDevice(
      const std::optional<std::string>& cache_guid) const override;
  void RecordTabGroupEvent(const EventDetails& event_details) override;

  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSavedTabGroupControllerDelegate() override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSharedTabGroupControllerDelegate() override;

  std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser() override;

  void AddObserver(TabGroupSyncService::Observer* observer) override;
  void RemoveObserver(TabGroupSyncService::Observer* observer) override;

  // For testing only.
  void SetIsInitializedForTesting(bool initialized) override;

 private:
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
  void SavedTabGroupModelLoaded() override;

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
  void HandleTabGroupRemoved(
      std::pair<base::Uuid, std::optional<LocalTabGroupID>> id_pair,
      TriggerSource source);
  void HandleTabGroupsReordered(TriggerSource source);

  // Read and write deleted local group IDs to disk. We add a local ID in
  // response to a group deletion event from sync. We clear that ID only when
  // RemoveLocalTabGroupMapping is invoked from the UI.
  // On startup, UI invokes GetDeletedGroupIdsFromPref to clean up any deleted
  // groups from tab model.
  std::vector<LocalTabGroupID> GetDeletedGroupIdsFromPref();
  void AddDeletedGroupIdToPref(const LocalTabGroupID& local_id,
                               const base::Uuid& sync_id);
  void RemoveDeletedGroupIdFromPref(const LocalTabGroupID& local_id);

  // Wrapper function that calls all metric recording functions.
  void RecordMetrics();

  // Force remove all tab groups that are currently not open in the local tab
  // model. This is used on a certain finch group in order to fix an earlier
  // bug.
  void ForceRemoveClosedTabGroupsOnStartup();

  // Helper function to update attributions for a group and optionally a tab.
  void UpdateAttributions(
      const LocalTabGroupID& group_id,
      const std::optional<LocalTabID>& tab_id = std::nullopt);

  // Helper function to log a tab group event in histograms.
  void LogEvent(TabGroupEvent event,
                LocalTabGroupID group_id,
                const std::optional<LocalTabID>& tab_id = std::nullopt);

  // The in-memory model representing the currently present saved tab groups.
  std::unique_ptr<SavedTabGroupModel> model_;

  // Sync bridges and data storage for both saved and shared tab group data.
  std::unique_ptr<TabGroupSyncBridgeMediator> sync_bridge_mediator_;

  // The UI coordinator to apply changes between local tab groups and the
  // TabGroupSyncService.
  std::unique_ptr<TabGroupSyncCoordinator> coordinator_;

  // Helper class for logging metrics.
  std::unique_ptr<TabGroupSyncMetricsLogger> metrics_logger_;

  // The pref service for storing migration status.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Whether the initialization has been completed, i.e. all the groups and the
  // ID mappings have been loaded into memory.
  bool is_initialized_ = false;

  // Groups with zero tabs are groups that still haven't received their tabs
  // from sync. UI can't handle these groups, hence the service needs to wait
  // before notifying the observers.
  std::set<base::Uuid> empty_groups_;

  // Obsevers of the model.
  base::ObserverList<TabGroupSyncService::Observer> observers_;

  // Keeps track of API calls received before the service is initialized.
  // Once the initialization is complete, these callbacks are run in the order
  // they were received. External observers are notified of init completion only
  // after this step. Currently used only for UpdateLocalTabGroupMapping()
  // calls.
  base::circular_deque<base::OnceClosure> pending_actions_;

  base::WeakPtrFactory<TabGroupSyncServiceImpl> weak_ptr_factory_{this};
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_IMPL_H_
