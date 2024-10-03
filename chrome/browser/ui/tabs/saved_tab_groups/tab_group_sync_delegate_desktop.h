// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_DELEGATE_DESKTOP_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_proxy.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"

class Browser;
class Profile;
class TabStripModel;

namespace tabs {
class TabModel;
}

namespace tab_groups {
class TabGroupSyncService;

// Desktop implementation of TabGroupSyncDelegate.
class TabGroupSyncDelegateDesktop : public TabGroupSyncDelegate {
 public:
  explicit TabGroupSyncDelegateDesktop(TabGroupSyncService* service,
                                       Profile* profile);
  ~TabGroupSyncDelegateDesktop() override;

  // TabGroupSyncDelegate implementation.
  void HandleOpenTabGroupRequest(
      const base::Uuid& sync_tab_group_id,
      std::unique_ptr<TabGroupActionContext> context) override;
  void CreateLocalTabGroup(const SavedTabGroup& tab_group) override;
  void CloseLocalTabGroup(const LocalTabGroupID& local_id) override;
  void DisconnectLocalTabGroup(const LocalTabGroupID& local_id) override;
  void UpdateLocalTabGroup(const SavedTabGroup& group) override;
  std::vector<LocalTabGroupID> GetLocalTabGroupIds() override;
  std::vector<LocalTabID> GetLocalTabIdsForTabGroup(
      const LocalTabGroupID& local_tab_group_id) override;
  void CreateRemoteTabGroup(const LocalTabGroupID& local_tab_group_id) override;
  std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser() override;

 private:
  // Opens the tabs in `saved_group` in `browser`. These tabs are not grouped.
  std::map<tabs::TabModel*, base::Uuid> OpenTabsAndMapToUuids(
      Browser* const browser,
      const SavedTabGroup& saved_group);

  // Adds the opened tabs from OpenTabsAndMapToUuids into a tab
  // group and links it to `saved_group`.
  TabGroupId AddOpenedTabsToGroup(
      TabStripModel* tab_strip_model,
      const std::map<tabs::TabModel*, base::Uuid>& opened_web_contents_to_uuid,
      const SavedTabGroup& saved_group);

  // The service used to query and manage SavedTabGroups.
  raw_ptr<TabGroupSyncService> service_ = nullptr;

  // Listener layer which observes and manages the state of open SavedTabGroups
  // across browsers.
  std::unique_ptr<SavedTabGroupModelListener> listener_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_DELEGATE_DESKTOP_H_
