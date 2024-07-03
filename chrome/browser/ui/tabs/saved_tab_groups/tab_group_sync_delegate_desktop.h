// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_DELEGATE_DESKTOP_H_

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/types.h"

namespace tab_groups {
class TabGroupSyncService;

// Desktop implementation of TabGroupSyncDelegate.
class TabGroupSyncDelegateDesktop : public TabGroupSyncDelegate {
 public:
  explicit TabGroupSyncDelegateDesktop(TabGroupSyncService* service);
  ~TabGroupSyncDelegateDesktop() override;

  // TabGroupSyncDelegate implementation.
  void HandleOpenTabGroupRequest(
      const base::Uuid& sync_tab_group_id,
      std::unique_ptr<TabGroupActionContext> context) override;
  void CreateLocalTabGroup(const SavedTabGroup& tab_group) override;
  void CloseLocalTabGroup(const LocalTabGroupID& local_id) override;
  void UpdateLocalTabGroup(const SavedTabGroup& group) override;
  std::vector<LocalTabGroupID> GetLocalTabGroupIds() override;
  std::vector<LocalTabID> GetLocalTabIdsForTabGroup(
      const LocalTabGroupID& local_tab_group_id) override;
  void CreateRemoteTabGroup(const LocalTabGroupID& local_tab_group_id) override;

 private:
  raw_ptr<TabGroupSyncService> service_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_DELEGATE_DESKTOP_H_
