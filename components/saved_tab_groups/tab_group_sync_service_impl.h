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

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/shared_tab_group_data_sync_bridge.h"
#include "components/saved_tab_groups/tab_group_store.h"
#include "components/saved_tab_groups/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/model/sync_data.h"

class PrefService;

namespace tab_groups {

// The internal implementation of the TabGroupSyncService.
class TabGroupSyncServiceImpl : public TabGroupSyncService,
                                public SavedTabGroupModelObserver {
 public:
  // Configuration for a specific sync data type.
  struct SyncDataTypeConfiguration {
    SyncDataTypeConfiguration(
        std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
        syncer::OnceModelTypeStoreFactory model_type_store_factory);
    ~SyncDataTypeConfiguration();

    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor;
    syncer::OnceModelTypeStoreFactory model_type_store_factory;
  };

  // `saved_tab_group_configuration` must not be `nullptr`.
  // `shared_tab_group_configuration` should be provided if feature is enabled.
  TabGroupSyncServiceImpl(
      std::unique_ptr<SavedTabGroupModel> model,
      std::unique_ptr<SyncDataTypeConfiguration> saved_tab_group_configuration,
      std::unique_ptr<SyncDataTypeConfiguration> shared_tab_group_configuration,
      std::unique_ptr<TabGroupStore> tab_group_store,
      PrefService* pref_service,
      std::map<base::Uuid, LocalTabGroupID> migrated_android_local_ids,
      std::unique_ptr<TabGroupSyncMetricsLogger> metrics_logger);
  ~TabGroupSyncServiceImpl() override;

  // Disallow copy/assign.
  TabGroupSyncServiceImpl(const TabGroupSyncServiceImpl&) = delete;
  TabGroupSyncServiceImpl& operator=(const TabGroupSyncServiceImpl&) = delete;

  // TabGroupSyncService implementation.
  void AddGroup(SavedTabGroup group) override;
  void RemoveGroup(const LocalTabGroupID& local_id) override;
  void RemoveGroup(const base::Uuid& sync_id) override;
  void UpdateVisualData(
      const LocalTabGroupID local_group_id,
      const tab_groups::TabGroupVisualData* visual_data) override;
  void AddTab(const LocalTabGroupID& group_id,
              const LocalTabID& tab_id,
              const std::u16string& title,
              GURL url,
              std::optional<size_t> position) override;
  void UpdateTab(const LocalTabGroupID& group_id,
                 const LocalTabID& tab_id,
                 const std::u16string& title,
                 GURL url,
                 std::optional<size_t> position) override;
  void RemoveTab(const LocalTabGroupID& group_id,
                 const LocalTabID& tab_id) override;
  void MoveTab(const LocalTabGroupID& group_id,
               const LocalTabID& tab_id,
               int new_group_index) override;
  void OnTabSelected(const LocalTabGroupID& group_id,
                     const LocalTabID& tab_id) override;

  std::vector<SavedTabGroup> GetAllGroups() override;
  std::optional<SavedTabGroup> GetGroup(const base::Uuid& guid) override;
  std::optional<SavedTabGroup> GetGroup(LocalTabGroupID& local_id) override;
  std::vector<LocalTabGroupID> GetDeletedGroupIds() override;
  void UpdateLocalTabGroupMapping(const base::Uuid& sync_id,
                                  const LocalTabGroupID& local_id) override;
  void RemoveLocalTabGroupMapping(const LocalTabGroupID& local_id) override;
  void UpdateLocalTabId(const LocalTabGroupID& local_group_id,
                        const base::Uuid& sync_tab_id,
                        const LocalTabID& local_tab_id) override;

  bool IsRemoteDevice(
      const std::optional<std::string>& cache_guid) const override;
  void RecordTabGroupEvent(const EventDetails& event_details) override;

  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSavedTabGroupControllerDelegate() override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSharedTabGroupControllerDelegate() override;

  void AddObserver(TabGroupSyncService::Observer* observer) override;
  void RemoveObserver(TabGroupSyncService::Observer* observer) override;

 private:
  // KeyedService:
  void Shutdown() override;

  // SavedTabGroupModelObserver implementation.
  void SavedTabGroupAddedFromSync(const base::Uuid& guid) override;
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;
  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;
  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup* removed_group) override;
  void SavedTabGroupRemovedLocally(const SavedTabGroup* removed_group) override;
  void SavedTabGroupLocalIdChanged(const base::Uuid& saved_group_id) override;
  void SavedTabGroupModelLoaded() override;

  // Called on reading ID mapping from tab group store.
  void OnReadTabGroupStore();

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

  // Stores SavedTabGroup data to the disk and to sync if enabled.
  SavedTabGroupSyncBridge saved_bridge_;

  // Stores SharedTabGroupData to the disk and to sync if enabled.
  std::unique_ptr<SharedTabGroupDataSyncBridge> shared_bridge_;

  // Stores tab group ID mapping (Sync ID -> Local ID) and some local metadata.
  std::unique_ptr<TabGroupStore> tab_group_store_;

  // The pref service for storing migration status.
  raw_ptr<PrefService> pref_service_;

  // Helper class for logging metrics.
  std::unique_ptr<TabGroupSyncMetricsLogger> metrics_logger_;

  // Whether the initialization has been completed, i.e. all the groups and the
  // ID mappings have been loaded into memory.
  bool is_initialized_ = false;

  // Keeps track of the ids of session restored tab groups that were once saved
  // in order to link them together again once the SavedTabGroupModelLoaded is
  // called. After the model is loaded, this variable is emptied to conserve
  // memory.
  std::vector<std::pair<base::Uuid, LocalTabGroupID>>
      saved_guid_to_local_group_id_mapping_;

  // Groups with zero tabs are groups that still haven't received their tabs
  // from sync. UI can't handle these groups, hence the service needs to wait
  // before notifying the observers.
  std::set<base::Uuid> empty_groups_;

  // Obsevers of the model.
  base::ObserverList<TabGroupSyncService::Observer> observers_;

  base::WeakPtrFactory<TabGroupSyncServiceImpl> weak_ptr_factory_{this};
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_IMPL_H_
