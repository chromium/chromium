// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_METRICS_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_METRICS_H_

class TabStripModel;

// This enum must match the numbering for SuggestedTabStripOrganizationReason in
// tools/metrics/histograms/enums.xml. Do not reorder or remove items. Add new
// items to the end and reflect them in the histogram enum.
enum class SuggestedTabStripOrganizationReason {
  MANUAL_TAB_GROUPING = 0,
  DRAGGED_WITHIN_SAME_TABSTRIP = 1,
  ADDED_TO_READING_LIST = 2,
  ADDED_TO_BOOKMARKS = 3,
  OPENED_LINK_IN_BACKGROUND = 4,
  SAME_ORIGIN_NAVIGATION = 5,
  SESSION_RESTORED = 6,
  RESUMING_FROM_STANDBY = 7,
};

void LogTabStripOrganizationUKM(const TabStripModel* model,
                                SuggestedTabStripOrganizationReason reason);

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_METRICS_H_
