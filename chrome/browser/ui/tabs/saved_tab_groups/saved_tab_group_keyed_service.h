// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_KEYED_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/internal/tab_group_sync_bridge_mediator.h"
#include "components/saved_tab_groups/public/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/gfx/range/range.h"

class Profile;
class TabGroup;

namespace tabs {
class TabModel;
}

namespace syncer {
class DeviceInfoTracker;
}

namespace tab_groups {

class TabGroupSyncServiceProxy;

// Serves to instantiate and own the SavedTabGroup infrastructure for the
// browser.
class SavedTabGroupKeyedService : public KeyedService,
                                  public SavedTabGroupController,
                                  public SavedTabGroupModelObserver {
 public:
  explicit SavedTabGroupKeyedService(
      Profile* profile,
      syncer::DeviceInfoTracker* device_info_tracker);
  SavedTabGroupKeyedService(const SavedTabGroupKeyedService&) = delete;
  SavedTabGroupKeyedService& operator=(const SavedTabGroupKeyedService& other) =
      delete;
  ~SavedTabGroupKeyedService() override;

  SavedTabGroupModelListener* listener() { return listener_.get(); }
  const SavedTabGroupModel* model() const { return model_.get(); }
  SavedTabGroupModel* model() { return model_.get(); }
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSavedTabGroupControllerDelegate();
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSharedTabGroupControllerDelegate();
  Profile* profile() { return profile_; }

  // SavedTabGroupController
  std::optional<tab_groups::TabGroupId> OpenSavedTabGroupInBrowser(
      Browser* browser,
      const base::Uuid saved_group_guid,
      tab_groups::OpeningSource opening_source) override;
  base::Uuid SaveGroup(const tab_groups::TabGroupId& group_id,
                       bool is_pinned = false) override;
  void UnsaveGroup(const tab_groups::TabGroupId& group_id,
                   ClosingSource closing_source) override;
  void PauseTrackingLocalTabGroup(
      const tab_groups::TabGroupId& group_id) override;
  void ResumeTrackingLocalTabGroup(
      const base::Uuid& saved_group_guid,
      const tab_groups::TabGroupId& group_id) override;
  void DisconnectLocalTabGroup(const tab_groups::TabGroupId& group_id) override;
  void ConnectLocalTabGroup(const tab_groups::TabGroupId& group_id,
                            const base::Uuid& saved_group_guid) override;

  // SavedTabGroupModelObserver
  void SavedTabGroupModelLoaded() override;
  void SavedTabGroupRemovedFromSync(const SavedTabGroup& group) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;

  // Connects local tab group to the saved guid from session restore.
  // This can be called prior to the saved tab group model is loaded or
  // that the `saved_guid` could no longer be present in the model.
  void ConnectRestoredGroupToSaveId(
      const base::Uuid& saved_guid,
      const tab_groups::TabGroupId local_group_id);

  // Saves a restored group. This can be called prior to the saved tab
  // group model is loaded. These groups are saved when the model is loaded.
  // This method will also connect an opened tab group to the saved group if the
  // local id is provided via `group`.
  void SaveRestoredGroup(SavedTabGroup group);

  void UpdateAttributions(
      const LocalTabGroupID& group_id,
      const std::optional<LocalTabID>& tab_id = std::nullopt);

  std::optional<std::string> GetLocalCacheGuid() const;

  // Helper function used to pause and resume tracking of all objects stored in
  // `listener_`. This is an RAII object which pauses tracking on construction,
  // and resumes tracking on destruction.
  std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser();

  void OnTabAddedToGroupLocally(const base::Uuid& group_guid);

  void OnTabRemovedFromGroupLocally(const base::Uuid& group_guid,
                                    const base::Uuid& tab_guid);

  void OnTabNavigatedLocally(const base::Uuid& group_guid,
                             const base::Uuid& tab_guid);

  void OnTabsReorderedLocally(const base::Uuid& group_guid);

  void OnTabGroupVisualsChanged(const base::Uuid& group_guid);

  TabGroupSyncServiceProxy* proxy() { return service_proxy_.get(); }
  TabGroupSyncMetricsLogger* GetTabGroupSyncMetricsLogger() {
    return metrics_logger_.get();
  }

 private:
  // Adds tabs to `tab_group` if `saved_group` was modified and has more tabs
  // than `tab_group` when a restore happens.
  void AddMissingTabsToOutOfSyncLocalTabGroup(
      Browser* browser,
      const TabGroup* const tab_group,
      const SavedTabGroup* const saved_group);

  // Remove tabs from `tab_group` if `saved_group` was modified and has less
  // tabs than `tab_group` when a restore happens.
  void RemoveExtraTabsFromOutOfSyncLocalTabGroup(
      TabStripModel* tab_strip_model,
      TabGroup* const tab_group,
      const SavedTabGroup* const saved_group);

  // Updates the tabs in the range `tab_range` to match the URLs of the
  // SavedTabGroupTabs in `saved_group`.
  void UpdateWebContentsToMatchSavedTabGroupTabs(
      const TabStripModel* const tab_strip_model,
      const SavedTabGroup* const saved_group,
      const gfx::Range& tab_range);

  // Given a `tab_range` which is indexes of tabs in the `tab_strip_model` that
  // should be part of `saved_group` outputs a mapping of tab to guid. This
  // method performs no checks to make sure that the tabs are the correct ones
  // that should be connected.
  std::map<tabs::TabModel*, base::Uuid> GetTabToGuidMappingForSavedGroup(
      const TabStripModel* const tab_strip_model,
      const SavedTabGroup* const saved_group,
      const gfx::Range& tab_range);

  // Opens a saved tab group into the tabstrip of the `browser` and returns a
  // mapping of the tabs to the SavedTabGroupTab. This method does not check
  // that the browser is accepting of groups so this must be done by callers.
  // This method does not check that the saved group is already open so that
  // must be done by callers. This method does not check if the saved_group is
  // part of the model, this must be done by callers.
  std::map<tabs::TabModel*, base::Uuid> OpenSavedTabGroupAndGetTabToGuidMapping(
      Browser* browser,
      const SavedTabGroup* const saved_group);

  // Helper method that takes the list of tabs that were created specifically
  // for the saved group, and then groups them in the tabstrip model.
  tab_groups::TabGroupId AddOpenedTabsToGroup(
      TabStripModel* const tab_strip_model_for_creation,
      const std::map<tabs::TabModel*, base::Uuid>& tab_guid_mapping,
      const SavedTabGroup& saved_group);

  // Returns a pointer to the TabStripModel which contains `local_group_id`.
  const TabStripModel* GetTabStripModelWithTabGroupId(
      const tab_groups::TabGroupId& local_group_id);

  // Returns the DataTypeStoreFactory tied to the current profile.
  syncer::OnceDataTypeStoreFactory GetStoreFactory();

  // Notifies observers that the tab group with id `group_id`'s visual data was
  // changed using data found in `saved_group_guid`.
  void UpdateGroupVisualData(base::Uuid saved_group_guid,
                             tab_groups::TabGroupId group_id);

  // Wrapper function that calls all metric recording functions.
  void RecordMetrics();

  // Records the Unsaved TabGroup count and the Tab count per Unsaved TabGroup.
  void RecordTabGroupMetrics();

  // Helper function to log a tab group event in histograms. This is implemented
  // in the same way as TabGroupSyncServiceImpl.
  void LogEvent(TabGroupEvent event,
                const base::Uuid& group_id,
                const std::optional<base::Uuid>& tab_id = std::nullopt);

  // The profile used to instantiate the keyed service.
  raw_ptr<Profile> profile_ = nullptr;

  // The current representation of this profiles saved tab groups.
  std::unique_ptr<SavedTabGroupModel> model_;

  // Proxy service which implements TabGroupSyncService. Forwards and translates
  // TabGroupSyncService calls to SavedTabGroupKeyedService calls.
  std::unique_ptr<TabGroupSyncServiceProxy> service_proxy_;

  // Listens to and observers all tabstrip models; updating the
  // SavedTabGroupModel when necessary.
  std::unique_ptr<SavedTabGroupModelListener> listener_;

  // Stores SavedTabGroup data to the disk and to sync if enabled.
  std::unique_ptr<TabGroupSyncBridgeMediator> sync_bridge_mediator_;

  // Helper class for logging metrics.
  std::unique_ptr<TabGroupSyncMetricsLogger> metrics_logger_;

  // Timer used to record periodic metrics about the state of the TabGroups
  // (saved and unsaved).
  base::RepeatingTimer metrics_timer_;

  // Keeps track of restored group to connect to model load.
  std::vector<std::pair<base::Uuid, tab_groups::TabGroupId>>
      restored_groups_to_connect_on_load_;

  // Keeps track of the groups to save on model load.
  std::vector<SavedTabGroup> restored_groups_to_save_on_load_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_KEYED_SERVICE_H_
