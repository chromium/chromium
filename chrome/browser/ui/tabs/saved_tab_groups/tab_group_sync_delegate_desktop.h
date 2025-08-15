// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_DELEGATE_DESKTOP_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"

class Browser;
class Profile;
class TabStripModel;

namespace tabs {
class TabInterface;
}

namespace tab_groups {

// Desktop implementation of TabGroupSyncDelegate.
class TabGroupSyncDelegateDesktop : public TabGroupSyncDelegate,
                                    TabGroupSyncService::Observer {
 public:
  explicit TabGroupSyncDelegateDesktop(TabGroupSyncService* service,
                                       Profile* profile);
  ~TabGroupSyncDelegateDesktop() override;

  // TabGroupSyncDelegate implementation.
  std::optional<LocalTabGroupID> HandleOpenTabGroupRequest(
      const base::Uuid& sync_tab_group_id,
      std::unique_ptr<TabGroupActionContext> context) override;
  void CreateLocalTabGroup(const SavedTabGroup& tab_group) override;
  void CloseLocalTabGroup(const LocalTabGroupID& local_id) override;
  void ConnectLocalTabGroup(const SavedTabGroup& group) override;
  void DisconnectLocalTabGroup(const LocalTabGroupID& local_id) override;
  void UpdateLocalTabGroup(const SavedTabGroup& group) override;
  std::vector<LocalTabGroupID> GetLocalTabGroupIds() override;
  std::vector<LocalTabID> GetLocalTabIdsForTabGroup(
      const LocalTabGroupID& local_tab_group_id) override;
  std::set<LocalTabID> GetSelectedTabs() override;
  std::u16string GetTabTitle(const LocalTabID& local_tab_id) override;
  std::unique_ptr<SavedTabGroup> CreateSavedTabGroupFromLocalGroup(
      const LocalTabGroupID& local_tab_group_id) override;
  std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser() override;

  // TabGroupSyncService::Observer
  void OnInitialized() override;
  void OnWillBeDestroyed() override;

  void StartRecordingHourlyMetrics();
  void RecordHourlyMetrics();

 private:
  // Opens the tabs in `saved_group` in `browser`. These tabs are not grouped.
  std::map<tabs::TabInterface*, base::Uuid> OpenTabsAndMapToUuids(
      Browser* const browser,
      const SavedTabGroup& saved_group);

  // Adds the opened tabs from OpenTabsAndMapToUuids into a tab
  // group and links it to `saved_group`.
  TabGroupId AddOpenedTabsToGroup(
      TabStripModel* tab_strip_model,
      const std::map<tabs::TabInterface*, base::Uuid>&
          opened_web_contents_to_uuid,
      const SavedTabGroup& saved_group);

  // The profile used to create this delegate.
  raw_ptr<Profile> profile_ = nullptr;

  // The service used to query and manage SavedTabGroups.
  raw_ptr<TabGroupSyncService> service_ = nullptr;

  // Listener layer which observes and manages the state of open SavedTabGroups
  // across browsers.
  std::unique_ptr<SavedTabGroupModelListener> listener_;

  // Logs a collection of metrics every hour.
  base::RepeatingTimer hourly_metrics_timer_;

  base::ScopedObservation<TabGroupSyncService, TabGroupSyncService::Observer>
      service_observation_{this};
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_DELEGATE_DESKTOP_H_
