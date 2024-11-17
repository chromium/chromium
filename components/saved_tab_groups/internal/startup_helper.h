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

// Helper class for handling parts of startup flow. Provides helper methods to
// 1. Initialize tab ID mappings for a saved tab group with the local group.
// 2. Cleanup deleted groups from the local tab model.
// 3. Add unsaved local groups to remote.
class StartupHelper {
 public:
  StartupHelper(TabGroupSyncDelegate* delegate, TabGroupSyncService* service);
  ~StartupHelper();

  // Disallow copy/assign.
  StartupHelper(const StartupHelper&) = delete;
  StartupHelper& operator=(const StartupHelper&) = delete;

  // Create tab ID mapping for a group based on left to right order.
  void MapTabIdsForGroup(const LocalTabGroupID& local_tab_group_id,
                         const SavedTabGroup& saved_tab_group);

  // Cleans up any tab groups from local tab model that have been already
  // deleted from sync.
  void CloseDeletedTabGroupsFromTabModel();

  // Creates saved tab groups for any unsaved group in the local tab model.
  void CreateRemoteTabGroupForNewGroups();

 private:
  // The service which represents remote from the point of view of this class.
  const raw_ptr<TabGroupSyncService> service_ = nullptr;

  // The platform specific delegate which represents local from the point of
  // view of this class.
  const raw_ptr<TabGroupSyncDelegate> platform_delegate_ = nullptr;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_STARTUP_HELPER_H_
