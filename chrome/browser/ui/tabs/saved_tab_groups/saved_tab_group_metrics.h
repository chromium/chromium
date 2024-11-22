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
  kOpenedFromSubmenu = 2,
  kClosed = 3,
  kMaxValue = kClosed
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:SharedTabGroupRecallTypeDesktop)

void RecordSharedTabGroupRecallType(SharedTabGroupRecallTypeDesktop action);

}  // namespace tab_groups::saved_tab_groups::metrics

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_METRICS_H_
