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
  kManualTabGrouping = 0,
  kDraggedWithinSameTabstrip = 1,
  kAddedToReadingList = 2,
  kAddedToBookmarks = 3,
  kOpenedLinkInBackground = 4,
  kSameOriginNavigation = 5,
  kSessionRestored = 6,
  kResumingFromStandby = 7,
  kMaxValue = kResumingFromStandby,
};

// This enum must match the numbering for TabOrganizationEntryPoint in
// tools/metrics/histograms/histograms.xml. Do not reorder or remove items. Add
// new items to the end and reflect them in the histogram enum.
enum class TabOrganizationEntryPoint {
  kNone = 0,
  kProactive = 1,
  kTabContextMenu = 2,
  kThreeDotMenu = 3,
  kTabSearch = 4,
  kMaxValue = kTabSearch,
};

void LogTabStripOrganizationUKM(const TabStripModel* model,
                                SuggestedTabStripOrganizationReason reason);

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_METRICS_H_
