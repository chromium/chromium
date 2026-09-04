// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRELOAD_SERVING_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRELOAD_SERVING_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/preload_serving_metrics_capsule.h"

namespace page_load_metrics_internal {

void RecordPreloadServingMetricsByNavigationInitiator(
    content::UsedInstantLoad used_instant_load,
    std::string_view navigation_initiator_string,
    bool is_url_srp);

void RecordFirstContentfulPaint(
    base::TimeDelta corrected_first_contentful_paint,
    bool is_in_foreground,
    content::UsedInstantLoad used_instant_load,
    std::string_view navigation_initiator_string,
    bool is_url_srp);

void RecordLargestContentfulPaint(
    base::TimeDelta corrected_largest_contentful_paint,
    content::UsedInstantLoad used_instant_load,
    std::string_view navigation_initiator_string,
    bool is_url_srp);

}  // namespace page_load_metrics_internal

// Records metrics using `PreloadServingMetrics`.
//
// See `PreloadServingMetrics` for more details.
class PreloadServingMetricsPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  PreloadServingMetricsPageLoadMetricsObserver();
  ~PreloadServingMetricsPageLoadMetricsObserver() override;

  // Not movable nor copyable.
  PreloadServingMetricsPageLoadMetricsObserver(
      PreloadServingMetricsPageLoadMetricsObserver&& other) = delete;
  PreloadServingMetricsPageLoadMetricsObserver& operator=(
      PreloadServingMetricsPageLoadMetricsObserver&& other) = delete;
  PreloadServingMetricsPageLoadMetricsObserver(
      const PreloadServingMetricsPageLoadMetricsObserver&) = delete;
  PreloadServingMetricsPageLoadMetricsObserver& operator=(
      const PreloadServingMetricsPageLoadMetricsObserver&) = delete;

 private:
  // PageLoadMetricsObserver implementation:
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  PageLoadMetricsObserver::ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnEnterBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnRestoreFromBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      content::NavigationHandle* navigation_handle) override;

  void MaybeRecord();

  // Holds data for a single navigation (or BFCache restore) needed to record
  // metrics.
  //
  // Created on commit (or BFCache restore) and reset when entering BFCache or
  // after metrics are recorded.
  struct NavigationData {
    NavigationData();
    ~NavigationData();
    NavigationData(NavigationData&&);
    NavigationData& operator=(NavigationData&&);

    std::unique_ptr<content::PreloadServingMetricsCapsule>
        preload_serving_metrics_capsule;
    bool used_bfcache;
    std::string navigation_initiator_string;
    bool is_url_srp;
    bool is_served_by_legacy_search_prefetch;
  };

  static NavigationData CreateNavigationData(
      content::NavigationHandle* navigation_handle,
      bool used_bfcache);

  std::optional<NavigationData> navigation_data_;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRELOAD_SERVING_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_
