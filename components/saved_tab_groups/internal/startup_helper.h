// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_STARTUP_HELPER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_STARTUP_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"

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
  // 3. For each tab group in sync,
  //    a. Populate tab ID mapping. We only persist tab group ID mapping in
  //    storage. Tab IDs are mapped on startup in-memory.
  //    b. Reconcile local state to be same as sync considering sync to be
  //    authoritative. We could have lost a update event from sync while the
  //    window wasn't running.
  void InitializeTabGroupSync();

  // Create tab ID mapping for a group based on left to right order.
  void MapTabIdsForGroup(const LocalTabGroupID& local_tab_group_id,
                         const SavedTabGroup& saved_tab_group);

 private:
  void CloseDeletedTabGroupsFromTabModel();
  void CreateRemoteTabGroupForNewGroups();

  // The service which represents remote from the point of view of this class.
  const raw_ptr<TabGroupSyncService> service_ = nullptr;

  // The platform specific delegate which represents local from the point of
  // view of this class.
  const raw_ptr<TabGroupSyncDelegate> platform_delegate_ = nullptr;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_STARTUP_HELPER_H_
