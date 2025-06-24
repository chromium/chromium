// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_INITIALIZED_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_INITIALIZED_OBSERVER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace tab_groups {

// This observer will wait until the TabGroupSyncService has initialized before
// allowing us to perform any actions on the service such as adding or removing
// groups. This is due to the services startup being asynchronous as a side
// effect of  posting tasks to prevent reentrancy issues.
class TabGroupSyncServiceInitializedObserver
    : public tab_groups::TabGroupSyncService::Observer {
 public:
  explicit TabGroupSyncServiceInitializedObserver(TabGroupSyncService* service);
  TabGroupSyncServiceInitializedObserver(
      const TabGroupSyncServiceInitializedObserver&) = delete;
  TabGroupSyncServiceInitializedObserver& operator=(
      const TabGroupSyncServiceInitializedObserver& other) = delete;
  ~TabGroupSyncServiceInitializedObserver() override;

  void Wait();

 private:
  // tab_groups::TabGroupSyncService::Observer
  void OnInitialized() override;

  base::RunLoop run_loop_;
  bool loaded_ = false;
  raw_ptr<TabGroupSyncService> service_;
  base::ScopedObservation<TabGroupSyncService, TabGroupSyncService::Observer>
      observer_{this};
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_INITIALIZED_OBSERVER_H_
