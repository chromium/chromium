// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_IMPL_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "components/saved_tab_groups/tab_group_sync_service.h"

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace tab_groups {

// The internal implementation of the TabGroupSyncService.
class TabGroupSyncServiceImpl : public TabGroupSyncService,
                                public SavedTabGroupModelObserver {
 public:
  TabGroupSyncServiceImpl(
      std::unique_ptr<SavedTabGroupModel> model,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory model_type_store_factory);
  ~TabGroupSyncServiceImpl() override;

  // Disallow copy/assign.
  TabGroupSyncServiceImpl(const TabGroupSyncServiceImpl&) = delete;
  TabGroupSyncServiceImpl& operator=(const TabGroupSyncServiceImpl&) = delete;

  // TabGroupSyncService implementation.
  void AddGroup(const SavedTabGroup& group) override;
  void RemoveGroup(const LocalTabGroupID& local_id) override;
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
  std::vector<SavedTabGroup> GetAllGroups() override;
  std::optional<SavedTabGroup> GetGroup(const base::Uuid& guid) override;
  std::optional<SavedTabGroup> GetGroup(LocalTabGroupID& local_id) override;
  void UpdateLocalTabGroupId(const base::Uuid& sync_id,
                             const LocalTabGroupID& local_id) override;
  void UpdateLocalTabId(const LocalTabGroupID& local_group_id,
                        const base::Uuid& sync_tab_id,
                        const LocalTabID& local_tab_id) override;
  syncer::ModelTypeSyncBridge* bridge() override;
  void AddObserver(TabGroupSyncService::Observer* observer) override;
  void RemoveObserver(TabGroupSyncService::Observer* observer) override;

 private:
  // SavedTabGroupModelObserver implementation.
  void SavedTabGroupAddedFromSync(const base::Uuid& guid) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;
  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup* removed_group) override;
  void SavedTabGroupModelLoaded() override;

  // The in-memory model representing the currently present saved tab groups.
  std::unique_ptr<SavedTabGroupModel> model_;

  // Stores SavedTabGroup data to the disk and to sync if enabled.
  SavedTabGroupSyncBridge bridge_;

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
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_IMPL_H_
