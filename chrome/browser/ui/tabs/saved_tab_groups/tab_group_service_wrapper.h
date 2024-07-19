// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SERVICE_WRAPPER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SERVICE_WRAPPER_H_

#include <optional>
#include <string>
#include <vector>

#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "components/saved_tab_groups/types.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "url/gurl.h"

namespace tab_groups {
class SavedTabGroupKeyedService;

// This class serves to hold pointers to and utilize the TabGroupSyncService and
// SavedTabGroupKeyedService. When
// tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled() is true we will
// use the TabGroupSyncService. Otherwise, we will default to the
// SavedTabGroupKeyedService. This class should be kept around until the full
// migration from SavedTabGroupKeyedService to TabGroupSyncService is completed.
// See crbug.com/350514491 for change-lists related to this effort.
class TabGroupServiceWrapper : public TabGroupSyncService {
 public:
  explicit TabGroupServiceWrapper(
      TabGroupSyncService* tab_group_sync_service,
      SavedTabGroupKeyedService* saved_tab_group_keyed_service);
  ~TabGroupServiceWrapper() override;

  // TabGroupSyncService implementation.
  void AddGroup(SavedTabGroup group) override;
  void RemoveGroup(const LocalTabGroupID& local_id) override;
  void RemoveGroup(const base::Uuid& sync_id) override;
  void UpdateVisualData(const LocalTabGroupID local_group_id,
                        const TabGroupVisualData* visual_data) override;
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

  bool IsRemoteDevice(
      const std::optional<std::string>& cache_guid) const override;
  void RecordTabGroupEvent(const EventDetails& event_details) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSavedTabGroupControllerDelegate() override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSharedTabGroupControllerDelegate() override;
  std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // These functions are only called for the SavedTabGroupKeyedService to log
  // metrics that the TabGroupSyncService is already recording.
  void OnTabAddedToGroupLocally(const base::Uuid& group_guid);
  void OnTabRemovedFromGroupLocally(const base::Uuid& group_guid,
                                    const base::Uuid& tab_guid);
  void OnTabNavigatedLocally(const base::Uuid& group_guid,
                             const base::Uuid& tab_guid);
  void OnTabsReorderedLocally(const base::Uuid& group_guid);
  void OnTabGroupVisualsChanged(const base::Uuid& group_guid);

  // Used to manually set the favicon for a specific tab. Should only be used in
  // the `saved_keyed_service_` code paths.
  // TODO(crbug.com/348486163): Find a way to support favicons for the
  // sync_service_ code paths.
  void SetFaviconForTab(const LocalTabGroupID& group_id,
                        const LocalTabID& tab_id,
                        std::optional<gfx::Image> favicon);

 private:
  bool ShouldUseSyncService();

  // The new keyed service for SavedTabGroups which will replace the old
  // service after a migration. See crbug.com/350514491.
  raw_ptr<TabGroupSyncService> sync_service_;
  // This is the original keyed service for SavedTabGroups.
  raw_ptr<SavedTabGroupKeyedService> saved_keyed_service_;
};
}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SERVICE_WRAPPER_H_
