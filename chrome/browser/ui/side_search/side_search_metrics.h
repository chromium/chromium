// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_METRICS_H_
#define CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_METRICS_H_

// Enums for histograms:
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SideSearchAvailabilityChangeType {
  kBecomeAvailable = 0,
  kBecomeUnavailable = 1,
  kMaxValue = kBecomeUnavailable
};

enum class SideSearchPageActionLabelVisibility {
  kNotVisible = 0,
  kVisible = 1,
  kMaxValue = kVisible
};

enum class SideSearchNavigationType {
  kNavigationCommittedWithinSideSearch = 0,
  kRedirectionToTab = 1,
  kMaxValue = kRedirectionToTab
};
// End of enums for histograms.

void RecordSideSearchAvailabilityChanged(SideSearchAvailabilityChangeType type);
void RecordSideSearchPageActionLabelVisibilityOnToggle(
    SideSearchPageActionLabelVisibility label_visibility);
void RecordSideSearchNavigation(SideSearchNavigationType type);
void RecordNavigationCommittedWithinSideSearchCountPerJourney(
    bool is_side_contents_helper_created_from_menu_option,
    int count,
    bool was_auto_triggered);
void RecordRedirectionToTabCountPerJourney(
    bool is_side_contents_helper_created_from_menu_option,
    int count,
    bool was_auto_triggered);
void RecordSideSearchNumTimesReturnedBackToSRP(int count);

#endif  // CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_METRICS_H_
