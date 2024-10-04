// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_COORDINATOR_IMPL_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_COORDINATOR_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/internal/startup_helper.h"
#include "components/saved_tab_groups/internal/tab_group_sync_coordinator.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"

namespace tab_groups {

class TabGroupSyncDelegate;

// Implementation of TabGroupSyncCoordinator. See base class for documentation.
class TabGroupSyncCoordinatorImpl : public TabGroupSyncCoordinator {
 public:
  TabGroupSyncCoordinatorImpl(std::unique_ptr<TabGroupSyncDelegate> delegate,
                              TabGroupSyncService* service);
  ~TabGroupSyncCoordinatorImpl() override;

  // Disallow copy/assign.
  TabGroupSyncCoordinatorImpl(const TabGroupSyncCoordinatorImpl&) = delete;
  TabGroupSyncCoordinatorImpl& operator=(const TabGroupSyncCoordinatorImpl&) =
      delete;

  // TabGroupSyncCoordinator implementation.
  void HandleOpenTabGroupRequest(
      const base::Uuid& sync_tab_group_id,
      std::unique_ptr<TabGroupActionContext> context) override;
  void ConnectLocalTabGroup(const base::Uuid& sync_id,
                            const LocalTabGroupID& local_id,
                            OpeningSource opening_source) override;
  void DisconnectLocalTabGroup(const LocalTabGroupID& local_id) override;
  std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser() override;

  // TabGroupSyncService::Observer methods.
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
  // The service which represents remote from the point of view of this class.
  raw_ptr<TabGroupSyncService> service_ = nullptr;

  // The platform specific delegate which represents local from the point of
  // view of this class.
  std::unique_ptr<TabGroupSyncDelegate> platform_delegate_;

  // To help with the startup logic.
  std::unique_ptr<StartupHelper> startup_helper_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_COORDINATOR_IMPL_H_
