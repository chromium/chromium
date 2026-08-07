// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_SCENARIO_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_SCENARIO_H_

namespace page_load_metrics {

// Categorizes page load navigations based on high-level scenario
// classifications.
//
// Observers query this classification to slice metrics based on navigation
// scenarios such as browser startup, new window creation, or same-window
// navigations.
enum class NavigationScenario {
  kUnknown = 0,
  kStartup = 1,
  kNewWindow = 2,
  kSameWindow = 3,
  kMaxValue = kSameWindow,
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_SCENARIO_H_
