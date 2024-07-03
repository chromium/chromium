// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_STARTUP_HELPER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_STARTUP_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/types.h"

namespace tab_groups {

class TabGroupSyncDelegate;
class TabGroupSyncService;

// Handles startup flow. Invoked when both the local tab model and
// TabGroupSyncService have been initialized. Primarily reconciles remote
// tab group updates / deletions with the local model and local group
// additions to remote. Also initializes tab ID mappings for the session.
class StartupHelper {
 public:
  StartupHelper(TabGroupSyncDelegate* delegate, TabGroupSyncService* service);
  ~StartupHelper();

  // Disallow copy/assign.
  StartupHelper(const StartupHelper&) = delete;
  StartupHelper& operator=(const StartupHelper&) = delete;

  // The startup routine that is executed in order:
  //
  // 1. Delete any tab groups from tab model that were deleted from sync. It
  // could happen in multi-window situations where the deletion event was
  // received when the window wasn't alive.
  // 2. Add any tab group to sync that doesn't exist yet in sync. This is meant
  // to handle when tab group sync feature is turned on for the first time or
  // after a rollback.
  // 3. Reconcile local state to be same as sync. We could have lost a update
  // event from sync while the window wasn't running.
  // 4. Populate tab ID mapping for TabGroupSyncService. We only persist tab
  // group ID mapping in storage. Tab IDs are mapped on startup.
  void InitializeTabGroupSync();

 private:
  void CloseDeletedTabGroupsFromTabModel();
  void CreateRemoteTabGroupForNewGroups();
  void ReconcileGroupsToSync();
  void UpdateTabIdMappings();

  // The platform specific delegate which represents local from the point of
  // view of this class.
  const raw_ptr<TabGroupSyncDelegate> platform_delegate_;

  // The service which represents remote from the point of view of this class.
  const raw_ptr<TabGroupSyncService> service_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_STARTUP_HELPER_H_
