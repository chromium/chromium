// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_SEARCH_PREWARM_COVERAGE_STATUS_H_
#define COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_SEARCH_PREWARM_COVERAGE_STATUS_H_

namespace page_load_metrics {

// Records the process reuse and prewarm/prerender coverage status for Google
// Search navigations. Categorizes the full funnel of outcomes: whether a
// prerender was activated, a prewarmed process was reused, the current tab's
// renderer process was reused (with or without prewarm), an initial blank/empty
// process was reused (e.g. about:blank, NTP), another existing process was
// reused, or a cold process was allocated.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SearchPrewarmPrerenderCoverageStatus)
enum class SearchPrewarmPrerenderCoverageStatus {
  // =========================================================================
  // Tier 1: Prerender Activated (Instant Navigation)
  // =========================================================================
  // 0: Prerender activated.
  kPrerenderActivated = 0,

  // =========================================================================
  // Tier 2: Preload Process Reused
  // =========================================================================
  // 1: Preload (prewarm) process reused by primary search navigation.
  kPreloadProcessReused_Prewarm = 1,

  // =========================================================================
  // Tier 3: In-Tab / Current Process Reused (e.g. SRP -> SRP)
  // =========================================================================
  // 2: Primary nav REUSED the current tab's renderer process.
  kCurrentProcessReused = 2,

  // 3: Primary nav REUSED the current tab's renderer process, which was
  // originally prewarmed.
  kCurrentProcessReused_Prewarm = 3,

  // =========================================================================
  // Tier 4: Other Existing Process Reused (from pool or other tab)
  // =========================================================================
  // 4: Primary nav REUSED another existing renderer process.
  kOtherProcessReused = 4,

  // =========================================================================
  // Tier 5: Cold Process Allocated
  // =========================================================================
  // 5: Cold process allocated.
  kColdProcessAllocated = 5,

  // =========================================================================
  // Tier 6: Blank / Initial Empty Process Reused (e.g. about:blank, NTP)
  // =========================================================================
  // 6: Primary nav REUSED an initial uncommitted/empty renderer process
  // (e.g. about:blank or NTP).
  kBlankProcessReused = 6,

  kMaxValue = kBlankProcessReused,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:SearchPrewarmPrerenderCoverageStatus)

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_SEARCH_PREWARM_COVERAGE_STATUS_H_
