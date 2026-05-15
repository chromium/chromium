// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_SOFT_NAVIGATION_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_SOFT_NAVIGATION_PAGE_LOAD_METRICS_OBSERVER_H_

#include <cstdint>
#include <optional>

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace page_load_metrics {
enum class PageLoadType;
}  // namespace page_load_metrics
namespace ukm::builders {
class SoftNavigation;
}  // namespace ukm::builders

// This observer records the SoftNavigation events to UKM, for 'regular' page
// loads, prerendered and activated page loads, and back-forward cache restores.
class SoftNavigationPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  SoftNavigationPageLoadMetricsObserver();
  ~SoftNavigationPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  const char* GetObserverName() const override;

  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;

  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;

  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;

  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;

  ObservePolicy OnEnterBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnRestoreFromBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      content::NavigationHandle* navigation_handle) override;

  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  ObservePolicy OnShown() override;

  void OnSoftNavigation() override;

  // State machine to verify this observer's assumptions about page lifecycle.
  enum class State {
    kInitial,
    kStarted,
    kPrerenderStarted,
    kPrerenderActivated,
    kInBackForwardCache,
    kRestoredFromBackForwardCache,
    kComplete,
  };

 private:
  bool FromForegroundOptionalEventInForeground(
      const std::optional<base::TimeDelta>& event);
  void RecordSoftNavigationEventIfPending();
  void RecordSoftLcp(ukm::builders::SoftNavigation& builder);
  void RecordSoftInp(ukm::builders::SoftNavigation& builder);
  void RecordSoftCls(ukm::builders::SoftNavigation& builder);

  State state_ = State::kInitial;
  // We record soft navigations after they complete: (1) when the next soft
  // navigation arrives, (2) when the (hard) navigation completes, or *eagerly*
  // (3) when the app moves to the background (See
  // ::FlushMetricsOnAppEnterBackground). To not double record after recording
  // eagerly, this variable indicates that a soft navigation has been detected
  // but not yet been recorded.
  bool pending_soft_navigation_ = false;
  // Only record soft CLS if we were ever in the foreground.
  bool should_record_soft_cls_ = false;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_SOFT_NAVIGATION_PAGE_LOAD_METRICS_OBSERVER_H_
