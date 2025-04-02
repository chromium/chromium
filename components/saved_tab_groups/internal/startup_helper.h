// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_STARTUP_HELPER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_STARTUP_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"

class PrefService;

namespace tab_groups {

class TabGroupSyncDelegate;
class TabGroupSyncService;

// Helper class for handling parts of startup flow. Provides helper methods to
// 1. Initialize tab ID mappings for a saved tab group with the local group.
// 2. Cleanup deleted groups from the local tab model.
// 3. Add unsaved local groups to remote.
class StartupHelper {
 public:
  StartupHelper(TabGroupSyncDelegate* delegate,
                TabGroupSyncService* service,
                PrefService* pref_service);
  ~StartupHelper();

  // Disallow copy/assign.
  StartupHelper(const StartupHelper&) = delete;
  StartupHelper& operator=(const StartupHelper&) = delete;

  // Cleans up any tab groups from local tab model that have been already
  // deleted from sync.
  void CloseDeletedTabGroupsFromTabModel();

  // Handle any local tab groups that don't exist in sync. They will be either
  // closed or added back to sync depending on whether this is the first time
  // the sync feature is being enabled.
  void HandleUnsavedLocalTabGroups();

 private:
  // The platform specific delegate which represents local from the point of
  // view of this class.
  const raw_ptr<TabGroupSyncDelegate> platform_delegate_ = nullptr;

  // The service which represents remote from the point of view of this class.
  const raw_ptr<TabGroupSyncService> service_ = nullptr;

  // The pref service used for checking tab group migration status.
  const raw_ptr<PrefService> pref_service_ = nullptr;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_STARTUP_HELPER_H_
