// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_KEYED_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/gfx/range/range.h"

class Profile;
class TabGroup;

// Serves to instantiate and own the SavedTabGroup infrastructure for the
// browser.
class SavedTabGroupKeyedService : public KeyedService,
                                  public SavedTabGroupController,
                                  public SavedTabGroupModelObserver {
 public:
  explicit SavedTabGroupKeyedService(Profile* profile);
  SavedTabGroupKeyedService(const SavedTabGroupKeyedService&) = delete;
  SavedTabGroupKeyedService& operator=(const SavedTabGroupKeyedService& other) =
      delete;
  ~SavedTabGroupKeyedService() override;

  SavedTabGroupModelListener* listener() { return &listener_; }
  const SavedTabGroupModel* model() const { return &model_; }
  SavedTabGroupModel* model() { return &model_; }
  SavedTabGroupSyncBridge* bridge() { return &bridge_; }
  Profile* profile() { return profile_; }

  // Populates `saved_guid_to_local_group_id_mapping_` with a pair to link once
  // SavedTabGroupModelLoaded is called.
  void StoreLocalToSavedId(const base::Uuid& saved_guid,
                           const tab_groups::TabGroupId local_group_id);

  // SavedTabGroupController
  void OpenSavedTabGroupInBrowser(Browser* browser,
                                  const base::Uuid& saved_group_guid) override;
  void SaveGroup(const tab_groups::TabGroupId& group_id) override;
  void UnsaveGroup(const tab_groups::TabGroupId& group_id) override;
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
  void SavedTabGroupRemovedFromSync(const SavedTabGroup* group) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_guid,
      const absl::optional<base::Uuid>& tab_guid) override;

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

  // Connects all SavedTabGroupTabs from a SavedTabGroup to their respective
  // WebContents in the local TabGroup.
  std::map<content::WebContents*, base::Uuid>
  GetWebContentsToTabGuidMappingForSavedGroup(
      const TabStripModel* const tab_strip_model,
      const SavedTabGroup* const saved_group,
      const gfx::Range& tab_range);

  // Connects all SavedTabGroupTabs from a SavedTabGroup to their respective
  // WebContents that will open.
  std::map<content::WebContents*, base::Uuid>
  GetWebContentsToTabGuidMappingForOpening(
      Browser* browser,
      const SavedTabGroup* const saved_group,
      const base::Uuid& saved_group_guid);

  // Activates the first tab in saved group that is already opened when its
  // button is pressed.
  void FocusFirstTabInOpenGroup(tab_groups::TabGroupId local_group_id);

  // Returns a pointer to the TabStripModel which contains `local_group_id`.
  const TabStripModel* GetTabStripModelWithTabGroupId(
      const tab_groups::TabGroupId& local_group_id);

  // Returns the ModelTypeStoreFactory tied to the current profile.
  syncer::OnceModelTypeStoreFactory GetStoreFactory();

  // Notifies observers that the tab group with id `group_id`'s visual data was
  // changed using data found in `saved_group_guid`.
  void UpdateGroupVisualData(base::Uuid saved_group_guid,
                             tab_groups::TabGroupId group_id);

  // Wrapper function that calls all metric recording functions.
  void RecordMetrics();

  // Records the SavedTabGroup count and Tab count per SavedTabGroup.
  void RecordSavedTabGroupMetrics();

  // Records the Unsaved TabGroup count and the Tab count per Unsaved TabGroup.
  void RecordTabGroupMetrics();

  // The profile used to instantiate the keyed service.
  raw_ptr<Profile> profile_ = nullptr;

  // The current representation of this profiles saved tab groups.
  SavedTabGroupModel model_;

  // Listens to and observers all tabstrip models; updating the
  // SavedTabGroupModel when necessary.
  SavedTabGroupModelListener listener_;

  // Stores SavedTabGroup data to the disk and to sync if enabled.
  SavedTabGroupSyncBridge bridge_;

  // Timer used to record periodic metrics about the state of the TabGroups
  // (saved and unsaved).
  base::RepeatingTimer metrics_timer_;

  // Keeps track of the ids of session restored tab groups that were once saved
  // in order to link them together again once the SavedTabGroupModelLoaded is
  // called. After the model is loaded, this variable is emptied to conserve
  // memory.
  std::vector<std::pair<base::Uuid, tab_groups::TabGroupId>>
      saved_guid_to_local_group_id_mapping_;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_KEYED_SERVICE_H_
