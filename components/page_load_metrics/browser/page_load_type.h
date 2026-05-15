// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_TYPE_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_TYPE_H_

namespace page_load_metrics {

// For UKM recording, specifically for SoftNavigation:PageLoadType.
// This indicates the type of hard navigation that had occurred when
// a soft navigation is detected.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PageLoadType {
  // A "regular" navigation was started. Therefore, the
  // corresponding PageLoad event is recorded to UKM by
  // UkmPageLoadMetricsObserver.
  kPageLoad = 0,
  // The (hard) navigation was prerendered and then activated
  // before the soft navigation occurred. Therefore,
  // the corresponding PrerenderPageLoad event is recorded to UKM
  // by PrerenderPageLoadMetricsObserver.
  kPrerenderPageLoad = 1,
  // The page was restored from bfcache when the soft navigation
  // occurred. Therefore, the corresponding HistoryNavigation event
  // is recorded to UKM by BackForwardCachePageLoadMetricsObserver.
  kHistoryNavigation = 2,
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_TYPE_H_
