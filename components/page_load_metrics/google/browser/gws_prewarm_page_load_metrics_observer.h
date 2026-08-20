// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_GWS_PREWARM_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_GWS_PREWARM_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

extern const char kHistogramGWSPrewarmWasResponseCached[];

}  // namespace internal

// Observes prerendered Google Search Prewarm page loads and records metrics,
// including whether the page was served from HTTP cache.
class GWSPrewarmPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  GWSPrewarmPageLoadMetricsObserver();
  ~GWSPrewarmPageLoadMetricsObserver() override;

  GWSPrewarmPageLoadMetricsObserver(const GWSPrewarmPageLoadMetricsObserver&) =
      delete;
  GWSPrewarmPageLoadMetricsObserver& operator=(
      const GWSPrewarmPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;

  bool was_cached() const { return was_cached_; }
  bool is_prerendered() const { return is_prerendered_; }

 private:
  bool was_cached_ = false;
  bool is_prerendered_ = false;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_GWS_PREWARM_PAGE_LOAD_METRICS_OBSERVER_H_
