// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_FROM_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_FROM_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"
#include "content/public/browser/error_navigation_trigger.h"
#include "third_party/blink/public/common/navigation/impression.h"

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
  ObservePolicy OnNavigationHandleTimingUpdated(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo&
          failed_provisional_load_info) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  std::string GetHistogramPrefix() const override;

  // Log the navigation milestone timing information. This is triggered
  // either when we are navigating away from the page (i.e. `OnComplete` of the
  // `PageLoadMetricsObserver`) or when the navigation is being abandoned. For
  // non-terminal abandonments such as tab hidden or app backgrounded, we record
  // this field every time the abandonment happens so that we have an entry
  // recorded even if app / tab is killed which might not call `OnComplete` or
  // other abandonment logging code. Note that there may be more than one entry
  // per navigation if non-terminal abandonment is involved, as a terminal
  // abandonment may happen after recording and also non-terminal abandonment
  // may happen multiple times within a navigation, and we may pass succeeding
  // milestones after the last record.
  void LogTimingInformationMetrics();
  void LogUKMHistograms(AbandonReason abandon_reason,
                        NavigationMilestone milestone,
                        base::TimeTicks event_time,
                        base::TimeTicks relative_start_time) override;
  bool IsAllowedToLogUKM() const override;
  bool IsAllowedToLogUMA() const override;

  uint64_t redirect_num_ = 0;
  std::optional<blink::Impression> impression_;
  bool is_committed_ = false;
  std::optional<net::Error> net_error_;
  std::optional<content::ErrorNavigationTrigger> error_navigation_trigger_;

  base::TimeTicks second_redirect_request_start_time_;
  base::TimeTicks second_redirect_response_start_time_;

  std::optional<std::set<NavigationMilestone>> last_logged_ukm_milestones_;

  // The value of the URL category (which is configured by
  // `kBeaconLeakageLoggingCategoryPrefix` and defaults to "category") parameter
  // in the initial navigated url.
  std::optional<uint32_t> category_parameter_id_;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_FROM_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_
