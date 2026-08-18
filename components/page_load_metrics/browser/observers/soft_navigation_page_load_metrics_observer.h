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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// State machine to verify this observer's assumptions about page lifecycle.
//
// LINT.IfChange(SoftNavigationPageLoadMetricsObserverState)
enum class SoftNavigationPageLoadMetricsObserverState {
  kInitial = 0,
  kStarted = 1,
  kPrerenderStarted = 2,
  kPrerenderActivated = 3,
  kInBackForwardCache = 4,
  kRestoredFromBackForwardCache = 5,
  kComplete = 6,
  kMaxValue = kComplete,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:SoftNavigationPageLoadMetricsObserverState)

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

  void OnSoftNavigationCompleted(const page_load_metrics::SoftNavigationData&
                                     soft_navigation_data) override;

 private:
  bool FromForegroundOptionalEventInForeground(
      const std::optional<base::TimeDelta>& event);
  void RecordSoftLcp(
      ukm::builders::SoftNavigation& builder,
      const page_load_metrics::SoftNavigationData& soft_navigation_data);
  void RecordSoftInp(
      ukm::builders::SoftNavigation& builder,
      const page_load_metrics::SoftNavigationData& soft_navigation_data);
  void RecordSoftCls(
      ukm::builders::SoftNavigation& builder,
      const page_load_metrics::SoftNavigationData& soft_navigation_data);

  using State = SoftNavigationPageLoadMetricsObserverState;
  State state_ = State::kInitial;
  // Only record soft CLS if we were ever in the foreground.
  bool should_record_soft_cls_ = false;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_SOFT_NAVIGATION_PAGE_LOAD_METRICS_OBSERVER_H_
