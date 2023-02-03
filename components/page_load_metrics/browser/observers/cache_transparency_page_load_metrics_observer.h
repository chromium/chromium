// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CACHE_TRANSPARENCY_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CACHE_TRANSPARENCY_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace internal {

// Expose metrics for tests.
extern const char kHistogramCacheTransparencyFirstContentfulPaint[];
extern const char kHistogramCacheTransparencyLargestContentfulPaint[];
extern const char kHistogramCacheTransparencyInteractionToNextPaint[];
extern const char kHistogramCacheTransparencyCumulativeLayoutShift[];

}  // namespace internal

// If the Cache Transparency feature is enabled or pervasive payloads are
// inputted, this observer is used to record Cache Transparency-related
// page-load metrics.
class CacheTransparencyPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  CacheTransparencyPageLoadMetricsObserver();

  CacheTransparencyPageLoadMetricsObserver(
      const CacheTransparencyPageLoadMetricsObserver&) = delete;
  CacheTransparencyPageLoadMetricsObserver& operator=(
      const CacheTransparencyPageLoadMetricsObserver&) = delete;

  ~CacheTransparencyPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  void RecordTimingHistograms();
  bool IsCacheTransparencyEnabled();
  bool IsPervasivePayloadsEnabled();
  void RecordSubresourceLoad();

  absl::optional<bool> is_pervasive_payloads_enabled_;
  absl::optional<bool> is_cache_transparency_enabled_;
  bool logged_ukm_event_ = false;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CACHE_TRANSPARENCY_PAGE_LOAD_METRICS_OBSERVER_H_
