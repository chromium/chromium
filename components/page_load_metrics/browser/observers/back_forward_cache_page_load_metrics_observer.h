// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_BACK_FORWARD_CACHE_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_BACK_FORWARD_CACHE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

extern const char kHistogramFirstPaintAfterBackForwardCacheRestore[];
extern const char kHistogramFirstInputDelayAfterBackForwardCacheRestore[];
extern const char kHistogramCumulativeShiftScoreAfterBackForwardCacheRestore[];
extern const char
    kHistogramCumulativeShiftScoreMainFrameAfterBackForwardCacheRestore[];
extern const char kHistogramCumulativeShiftScoreAfterBackForwardCacheRestore[];
extern const base::Feature kBackForwardCacheEmitZeroSamplesForKeyMetrics;

}  // namespace internal

class BackForwardCachePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  BackForwardCachePageLoadMetricsObserver();
  ~BackForwardCachePageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
  OnEnterBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnRestoreFromBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      content::NavigationHandle* navigation_handle) override;
  void OnFirstPaintAfterBackForwardCacheRestoreInPage(
      const page_load_metrics::mojom::BackForwardCacheTiming& timing,
      size_t index) override;
  void OnFirstInputAfterBackForwardCacheRestoreInPage(
      const page_load_metrics::mojom::BackForwardCacheTiming& timing,
      size_t index) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  // Records the layout shift score after the page is restored from the back-
  // forward cache. This is called when the page is navigated away, i.e., when
  // the page enters to the cache, or the page is closed. In the first call, as
  // the page has not been in the back-forward cache yet, this doesn't record
  // the scores.
  void MaybeRecordLayoutShiftScoreAfterBackForwardCacheRestore(
      const page_load_metrics::mojom::PageLoadTiming& timing);

  // Returns the UKM source ID for index-th back-foward restore navigation.
  int64_t GetUkmSourceIdForBackForwardCacheRestore(size_t index) const;

  // Returns the UKM source ID for the last back-foward restore navigation.
  int64_t GetLastUkmSourceIdForBackForwardCacheRestore() const;

  // Whether the page is currently in the back-forward cache or not.
  bool in_back_forward_cache_ = false;

  // The layout shift score. These are recorded when the page is navigated away.
  // These serve as "deliminators" between back-forward cache navigations.
  base::Optional<double> last_main_frame_layout_shift_score_;
  base::Optional<double> last_layout_shift_score_;

  // IDs for the navigations when the page is restored from the back-forward
  // cache.
  std::vector<ukm::SourceId> back_forward_cache_navigation_ids_;

  DISALLOW_COPY_AND_ASSIGN(BackForwardCachePageLoadMetricsObserver);
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_BACK_FORWARD_CACHE_PAGE_LOAD_METRICS_OBSERVER_H_
