// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_metrics.h"

#include "base/metrics/histogram_functions.h"

void RecordSideSearchAvailabilityChanged(
    SideSearchAvailabilityChangeType type) {
  base::UmaHistogramEnumeration("SideSearch.AvailabilityChanged", type);
}

void RecordSideSearchOpenAction(SideSearchOpenActionType action) {
  base::UmaHistogramEnumeration("SideSearch.OpenAction", action);
}

void RecordSideSearchCloseAction(SideSearchCloseActionType action) {
  base::UmaHistogramEnumeration("SideSearch.CloseAction", action);
}

void RecordSideSearchNavigation(SideSearchNavigationType type) {
  base::UmaHistogramEnumeration("SideSearch.Navigation", type);
}

void RecordNavigationCommittedWithinSideSearchCountPerJourney(int count) {
  base::UmaHistogramCounts100(
      "SideSearch.NavigationCommittedWithinSideSearchCountPerJourney", count);
}

void RecordRedirectionToTabCountPerJourney(int count) {
  base::UmaHistogramCounts100("SideSearch.RedirectionToTabCountPerJourney",
                              count);
}
