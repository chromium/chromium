// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_FROM_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_FROM_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"

// This observer tracks page loads that are initiated through GWS and are
// subsequently abandoned by the user (e.g., closing the tab,
// navigating away) before fully loading. It collects metrics related to
// these abandoned loads, such as:
//
// - Time spent before abandonment.
// - Navigation milestone status at abandonment.
//
// It will later be recorded via UKM.
class FromGWSAbandonedPageLoadMetricsObserver
    : public AbandonedPageLoadMetricsObserver {
 public:
  FromGWSAbandonedPageLoadMetricsObserver();
  ~FromGWSAbandonedPageLoadMetricsObserver() override;
  FromGWSAbandonedPageLoadMetricsObserver(
      const FromGWSAbandonedPageLoadMetricsObserver&) = delete;
  FromGWSAbandonedPageLoadMetricsObserver& operator=(
      const FromGWSAbandonedPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;

 private:
  std::string GetHistogramPrefix() const override;

  void LogOnFinishedMetrics(bool is_commited);
  void LogUKMHistograms(AbandonReason abandon_reason,
                        NavigationMilestone milestone,
                        base::TimeTicks event_time,
                        base::TimeTicks relative_start_time) override;
  bool IsAllowedToLogUKM() const override;
  bool IsAllowedToLogUMA() const override;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_FROM_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_
