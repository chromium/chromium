// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_COORDINATOR_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_COORDINATOR_H_

#include <memory>

#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"

namespace tab_groups {

// Central class that owns the tab group sync logic between local tab model and
// TabGroupSyncService. The tab model interactions are done via a delegate.
// that has platform specific implementations. Any cross-platform logic should
// live in this class. In long-term this class will contain:
// 1. Pass through logic to notify the delegate about remote updates.
// 2. Startup helper to enforce local and remote groups be at the same state on
// startup.
class TabGroupSyncCoordinator : public TabGroupSyncService::Observer {
 public:
  // Requests to open a saved tab group. Invoked from UI.
  virtual void HandleOpenTabGroupRequest(
      const base::Uuid& sync_tab_group_id,
      std::unique_ptr<TabGroupActionContext> context) = 0;

  // Pass-through logic from TabGroupSyncService::ConnectLocalTabGroup.
  virtual void ConnectLocalTabGroup(const base::Uuid& sync_id,
                                    const LocalTabGroupID& local_id,
                                    OpeningSource opening_source) = 0;
  virtual void DisconnectLocalTabGroup(const LocalTabGroupID& local_id) = 0;
  virtual std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser() = 0;

  // TabGroupSyncService::Observer overrides.
  void OnInitialized() override = 0;
  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override = 0;
  void OnTabGroupUpdated(const SavedTabGroup& group,
                         TriggerSource source) override = 0;
  void OnTabGroupRemoved(const LocalTabGroupID& local_id,
                         TriggerSource source) override = 0;
  void OnTabGroupRemoved(const base::Uuid& sync_id,
                         TriggerSource source) override = 0;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_COORDINATOR_H_
