// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_METRICS_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_METRICS_H_

// Contains enums, functions used to record desktop specific metrics for saved
// tab groups.
namespace tab_groups::saved_tab_groups::metrics {

// Types of Recall UI actions a user can trigger.
// These values are persisted to logs. Entries should not be renumbered and
// number values should never be reused.
// LINT.IfChange(SharedTabGroupRecallTypeDesktop)
enum class SharedTabGroupRecallTypeDesktop {
  kOpenedFromBookmarksBar = 0,
  kOpenedFromEverythingMenu = 1,
  kOpenedFromSubmenu = 2,  // Replaced by the submenu options below.
  kClosed = 3,
  kOpenedFromSubmenuFromBookmarksBar = 4,
  kOpenedFromSubmenuFromEverythingMenu = 5,
  kOpenedFromSubmenuFromAppMenu = 6,
  kOpenedFromSubmenuFromMacSystemMenu = 7,
  kMaxValue = kOpenedFromSubmenuFromMacSystemMenu
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:SharedTabGroupRecallTypeDesktop)

// Types of Manage UI actions a user can trigger.
// These values are persisted to logs. Entries should not be renumbered and
// number values should never be reused.
// LINT.IfChange(SharedTabGroupManageTypeDesktop)
enum class SharedTabGroupManageTypeDesktop {
  kShareGroup = 0,
  kManageGroup = 1,
  kDeleteGroup = 2,
  kRecentActivity = 3,
  kManageGroupFromUserJoinNotification = 4,
  kMaxValue = kManageGroupFromUserJoinNotification
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:SharedTabGroupManageTypeDesktop)

// Different ways a user can open a saved tab group from a submenu.
// These values are persisted to logs. Entries should not be renumbered and
// number values should never be reused.
// LINT.IfChange(SavedTabGroupOpenedSubmenuDesktop)
enum class SavedTabGroupOpenedSubmenuDesktop {
  kBookmarksBar = 0,
  kEverythingMenu = 1,
  kAppMenu = 2,
  kMacSystemMenu = 3,
  kMaxValue = kMacSystemMenu
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:SavedTabGroupOpenedSubmenuDesktop)

void RecordSharedTabGroupRecallType(SharedTabGroupRecallTypeDesktop action);
void RecordSharedTabGroupManageType(SharedTabGroupManageTypeDesktop action);
void RecordSavedTabGroupOpenedSubmenu(
    SavedTabGroupOpenedSubmenuDesktop submenu_type);

}  // namespace tab_groups::saved_tab_groups::metrics

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_METRICS_H_
