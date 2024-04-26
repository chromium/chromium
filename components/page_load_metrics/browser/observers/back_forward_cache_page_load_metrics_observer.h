// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_BACK_FORWARD_CACHE_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_BACK_FORWARD_CACHE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/feature_list.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

class BackForwardCachePageLoadMetricsObserverTest;
namespace base {
class TickClock;
}

namespace internal {

extern const char
    kAverageUserInteractionLatencyOverBudget_MaxEventDuration_AfterBackForwardCacheRestore
        [];
extern const char kNumInteractions_AfterBackForwardCacheRestore[];
extern const char
    kSlowUserInteractionLatencyOverBudgetHighPercentile2_MaxEventDuration_AfterBackForwardCacheRestore
        [];
extern const char
    kSumOfUserInteractionLatencyOverBudget_MaxEventDuration_AfterBackForwardCacheRestore
        [];
extern const char
    kUserInteractionLatencyHighPercentile2_MaxEventDuration_AfterBackForwardCacheRestore
        [];
extern const char
    kWorstUserInteractionLatency_MaxEventDuration_AfterBackForwardCacheRestore
        [];

extern const char kHistogramFirstPaintAfterBackForwardCacheRestore[];
extern const char
    kHistogramFirstRequestAnimationFrameAfterBackForwardCacheRestore[];
extern const char
    kHistogramSecondRequestAnimationFrameAfterBackForwardCacheRestore[];
extern const char
    kHistogramThirdRequestAnimationFrameAfterBackForwardCacheRestore[];
extern const char kHistogramFirstInputDelayAfterBackForwardCacheRestore[];
extern const char kHistogramCumulativeShiftScoreAfterBackForwardCacheRestore[];
extern const char
    kHistogramCumulativeShiftScoreMainFrameAfterBackForwardCacheRestore[];
extern const char kHistogramCumulativeShiftScoreAfterBackForwardCacheRestore[];
BASE_DECLARE_FEATURE(kBackForwardCacheEmitZeroSamplesForKeyMetrics);

}  // namespace internal

class BackForwardCachePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  BackForwardCachePageLoadMetricsObserver();

  BackForwardCachePageLoadMetricsObserver(
      const BackForwardCachePageLoadMetricsObserver&) = delete;
  BackForwardCachePageLoadMetricsObserver& operator=(
      const BackForwardCachePageLoadMetricsObserver&) = delete;

  ~BackForwardCachePageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url,
      bool started_in_foreground) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnPrerenderStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
  OnEnterBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnRestoreFromBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      content::NavigationHandle* navigation_handle) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
  ShouldObserveMimeType(const std::string& mime_type) const override;
  void OnFirstPaintAfterBackForwardCacheRestoreInPage(
      const page_load_metrics::mojom::BackForwardCacheTiming& timing,
      size_t index) override;
  void OnRequestAnimationFramesAfterBackForwardCacheRestoreInPage(
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
  friend class ::BackForwardCachePageLoadMetricsObserverTest;

  // Records metrics related to the end of a page visit. This occurs either
  // when the observed page enters (or re-enters) the back-forward cache, or
  // when OnComplete is called on this observer while the page is not in
  // the back-forward cache, or if the app enters the background.
  void RecordMetricsOnPageVisitEnd(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      bool app_entering_background);

  // Records the layout shift score after the page is restored from the back-
  // forward cache. This is called when the page is navigated away, i.e., when
  // the page enters to the cache, or the page is closed. In the first call, as
  // the page has not been in the back-forward cache yet, this doesn't record
  // the scores.
  void MaybeRecordLayoutShiftScoreAfterBackForwardCacheRestore(
      const page_load_metrics::mojom::PageLoadTiming& timing);

  // Records the duration the page was in the foreground for when the page is
  // hidden, navigated away from, or closed, after it has been restored from the
  // back forward cache at least once.
  // Does nothing if the page has never been restored.
  void MaybeRecordForegroundDurationAfterBackForwardCacheRestore(
      const base::TickClock* clock,
      bool app_entering_background) const;

  // Records a page end reason when the page is navigated away from or closed,
  // after it has been restored from the back forward cache at least once.
  // Does nothing if the page has never been restored.
  void MaybeRecordPageEndAfterBackForwardCacheRestore(
      bool app_entering_background);

  // Recorded normalized responsiveness metrics after the page is restored from
  // the back-forward cache. This is called when the page is navigated away,
  // i.e., when the page enters to the cache, or the page is closed. In the
  // first call, as the page has not been in the back-forward cache yet, this
  // doesn't record the scores.
  void MaybeRecordNormalizedResponsivenessMetrics();

  // Returns the UKM source ID for index-th back-foward restore navigation.
  int64_t GetUkmSourceIdForBackForwardCacheRestore(size_t index) const;

  // Returns the UKM source ID for the last back-foward restore navigation.
  int64_t GetLastUkmSourceIdForBackForwardCacheRestore() const;

  // Whether the page has ever entered the back-forward cache.
  bool has_ever_entered_back_forward_cache_ = false;

  // Whether the page is currently in the back-forward cache or not.
  bool in_back_forward_cache_ = false;

  // True if the page was not in the foreground when restored from back-forward
  // cache, or was ever hidden. Resets to false if the page re-enters the
  // back-forward cache.
  bool was_hidden_ = false;

  // Whether the current set of page metrics (CLS, LCP, etc) have already been
  // logged due to the page being backgrounded. Used to avoid double-logging
  // these metrics. This value gets re-set to false if the page is restored
  // from the BFCache.
  bool page_metrics_logged_due_to_backgrounding_ = false;

  // TODO(crbug.com/40203717): Remove this when removing the DCHECK for lack of
  // page end metrics logging from the back forward page load metrics observer.
  bool logged_page_end_metrics_ = false;

  // The layout shift score. These are updated whenever the page is restored
  // from the back-forward cache.
  std::optional<double> restored_main_frame_layout_shift_score_;
  std::optional<double> restored_layout_shift_score_;

  // IDs for the navigations when the page is restored from the back-forward
  // cache.
  std::vector<ukm::SourceId> back_forward_cache_navigation_ids_;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_BACK_FORWARD_CACHE_PAGE_LOAD_METRICS_OBSERVER_H_
