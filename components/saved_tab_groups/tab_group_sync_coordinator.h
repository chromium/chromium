// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_COORDINATOR_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/startup_helper.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "components/saved_tab_groups/types.h"

namespace tab_groups {

class TabGroupSyncDelegate;

// Central class that owns the tab group sync logic between local tab model and
// TabGroupSyncService. The tab model interactions are done via a delegate.
// that has platform specific implementations. Any cross-platform logic should
// live in this class. In long-term this class will contain:
// 1. Pass through logic to notify the delegate about remote updates.
// 2. Startup helper to enforce local and remote groups be at the same state on
// startup.
class TabGroupSyncCoordinator final : public TabGroupSyncService::Observer {
 public:
  TabGroupSyncCoordinator(std::unique_ptr<TabGroupSyncDelegate> delegate,
                          TabGroupSyncService* service);
  ~TabGroupSyncCoordinator() override;

  // Requests to open a saved tab group. Invoked from UI.
  void HandleOpenTabGroupRequest(
      const base::Uuid& sync_tab_group_id,
      std::unique_ptr<TabGroupActionContext> context);

  // TabGroupSyncService::Observer implementation.
  void OnInitialized() override;
  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override;
  void OnTabGroupUpdated(const SavedTabGroup& group,
                         TriggerSource source) override;
  void OnTabGroupRemoved(const LocalTabGroupID& local_id,
                         TriggerSource source) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id,
                         TriggerSource source) override;

 private:
  // The platform specific delegate which represents local from the point of
  // view of this class.
  std::unique_ptr<TabGroupSyncDelegate> platform_delegate_;

  // The service which represents remote from the point of view of this class.
  const raw_ptr<TabGroupSyncService> service_;

  // To help with the startup logic.
  StartupHelper startup_helper_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_COORDINATOR_H_
