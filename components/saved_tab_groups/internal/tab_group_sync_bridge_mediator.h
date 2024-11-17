// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_BRIDGE_MEDIATOR_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_BRIDGE_MEDIATOR_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/internal/sync_bridge_tab_group_model_wrapper.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"

class PrefService;

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace tab_groups {

class SavedTabGroupModel;
class SavedTabGroupSyncBridge;
class SharedTabGroupDataSyncBridge;

struct SyncDataTypeConfiguration;

// Used to control sync bridges for Saved and Shared tab groups and dispatch
// changes in the model to a corresponding bridge.
class TabGroupSyncBridgeMediator : public SavedTabGroupModelObserver {
 public:
  // `model` and `pref_service` must never be null and must outlive the current
  // object.
  TabGroupSyncBridgeMediator(
      SavedTabGroupModel* model,
      PrefService* pref_service,
      std::unique_ptr<SyncDataTypeConfiguration> saved_tab_group_configuration,
      std::unique_ptr<SyncDataTypeConfiguration>
          shared_tab_group_configuration);
  TabGroupSyncBridgeMediator(const TabGroupSyncBridgeMediator&) = delete;
  TabGroupSyncBridgeMediator& operator=(const TabGroupSyncBridgeMediator&) =
      delete;
  ~TabGroupSyncBridgeMediator() override;

  // Delegates for sync initialization.
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSavedTabGroupControllerDelegate();
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSharedTabGroupControllerDelegate();

  // SavedTabGroupSyncBridge specific getters.
  bool IsSavedBridgeSyncing() const;
  std::optional<std::string> GetLocalCacheGuidForSavedBridge() const;
  std::optional<std::string> GetAccountIdForSavedBridge() const;

  // SavedTabGroupModelObserver overrides.
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override;
  void SavedTabGroupRemovedLocally(const SavedTabGroup& removed_group) override;
  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;
  void SavedTabGroupTabMovedLocally(const base::Uuid& group_guid,
                                    const base::Uuid& tab_guid) override;
  void SavedTabGroupReorderedLocally() override;
  void SavedTabGroupLocalIdChanged(const base::Uuid& group_guid) override;
  void SavedTabGroupLastUserInteractionTimeUpdated(
      const base::Uuid& group_guid) override;

 private:
  // Populates loaded entries to the model when all data is loaded.
  void InitializeModelIfReady();

  void OnSavedGroupsWithTabsLoaded(std::vector<SavedTabGroup> groups,
                                   std::vector<SavedTabGroupTab> tabs);
  void OnSharedGroupsWithTabsLoaded(std::vector<SavedTabGroup> groups,
                                    std::vector<SavedTabGroupTab> tabs);

  raw_ptr<SavedTabGroupModel> model_;

  // Observes the SavedTabGroupModel.
  base::ScopedObservation<SavedTabGroupModel, SavedTabGroupModelObserver>
      observation_{this};

  SyncBridgeTabGroupModelWrapper saved_bridge_model_wrapper_;
  std::unique_ptr<SavedTabGroupSyncBridge> saved_bridge_;

  SyncBridgeTabGroupModelWrapper shared_bridge_model_wrapper_;
  std::unique_ptr<SharedTabGroupDataSyncBridge> shared_bridge_;

  // Temporary storage of groups and tabs loaded from the disk for both saved
  // and shared tab groups. Empty once the model is initialized.
  std::vector<SavedTabGroup> loaded_saved_groups_;
  std::vector<SavedTabGroupTab> loaded_saved_tabs_;
  std::vector<SavedTabGroup> loaded_shared_groups_;
  std::vector<SavedTabGroupTab> loaded_shared_tabs_;
  bool saved_tab_groups_loaded_ = false;
  bool shared_tab_groups_loaded_ = false;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_BRIDGE_MEDIATOR_H_
