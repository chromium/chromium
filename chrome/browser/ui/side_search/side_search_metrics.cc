// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_metrics.h"

#include "base/metrics/histogram_functions.h"

void RecordSideSearchAvailabilityChanged(
    SideSearchAvailabilityChangeType type) {
  base::UmaHistogramEnumeration("SideSearch.AvailabilityChanged", type);
}

void RecordSideSearchPageActionLabelVisibilityOnToggle(
    SideSearchPageActionLabelVisibility label_visibility) {
  base::UmaHistogramEnumeration(
      "SideSearch.PageActionIcon.LabelVisibleWhenToggled", label_visibility);
}

void RecordSideSearchNavigation(SideSearchNavigationType type) {
  base::UmaHistogramEnumeration("SideSearch.Navigation", type);
}

void RecordNavigationCommittedWithinSideSearchCountPerJourney(
    bool is_side_contents_helper_created_from_menu_option,
    int count,
    bool was_auto_triggered) {
  if (is_side_contents_helper_created_from_menu_option) {
    base::UmaHistogramCounts100(
        "SideSearch."
        "NavigationCommittedWithinSideSearchCountPerJourneyFromMenuOption",
        count);
  } else if (was_auto_triggered) {
    base::UmaHistogramCounts100(
        "SideSearch.AutoTrigger."
        "NavigationCommittedWithinSideSearchCountPerJourney",
        count);
  }
  base::UmaHistogramCounts100(
      "SideSearch.NavigationCommittedWithinSideSearchCountPerJourney2", count);
}

void RecordRedirectionToTabCountPerJourney(
    bool is_side_contents_helper_created_from_menu_option,
    int count,
    bool was_auto_triggered) {
  if (is_side_contents_helper_created_from_menu_option) {
    base::UmaHistogramCounts100(
        "SideSearch.RedirectionToTabCountPerJourneyFromMenuOption", count);
  } else if (was_auto_triggered) {
    base::UmaHistogramCounts100(
        "SideSearch.AutoTrigger.RedirectionToTabCountPerJourney", count);
  }
  base::UmaHistogramCounts100("SideSearch.RedirectionToTabCountPerJourney2",
                              count);
}

void RecordSideSearchNumTimesReturnedBackToSRP(int count) {
  base::UmaHistogramCounts100("SideSearch.TimesReturnedBackToSRP", count);
}
