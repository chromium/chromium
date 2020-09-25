// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_UMA_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_UMA_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/observers/click_input_tracker.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace internal {

// NOTE: Some of these histograms are separated into a separate histogram
// specified by the ".Background" suffix. For these events, we put them into the
// background histogram if the web contents was ever in the background from
// navigation start to the event in question.
extern const char kHistogramFirstInputDelay[];
extern const char kHistogramFirstInputTimestamp[];
extern const char kHistogramFirstInputDelay4[];
extern const char kHistogramFirstInputTimestamp4[];
extern const char kHistogramLongestInputDelay[];
extern const char kHistogramLongestInputTimestamp[];
extern const char kHistogramFirstPaint[];
extern const char kHistogramFirstImagePaint[];
extern const char kHistogramDomContentLoaded[];
extern const char kHistogramLoad[];
extern const char kHistogramFirstContentfulPaint[];
extern const char kHistogramFirstMeaningfulPaint[];
extern const char kHistogramLargestContentfulPaint[];
extern const char kHistogramLargestContentfulPaintContentType[];
extern const char kHistogramLargestContentfulPaintMainFrame[];
extern const char kHistogramLargestContentfulPaintMainFrameContentType[];
extern const char kHistogramExperimentalLargestContentfulPaint[];
extern const char kHistogramExperimentalLargestContentfulPaintContentType[];
extern const char kHistogramExperimentalLargestContentfulPaintMainFrame[];
extern const char
    kHistogramExperimentalLargestContentfulPaintMainFrameContentType[];
extern const char kHistogramParseDuration[];
extern const char kHistogramParseBlockedOnScriptLoad[];
extern const char kHistogramParseBlockedOnScriptExecution[];
extern const char kHistogramParseStartToFirstMeaningfulPaint[];

extern const char kBackgroundHistogramFirstImagePaint[];
extern const char kBackgroundHistogramDomContentLoaded[];
extern const char kBackgroundHistogramLoad[];
extern const char kBackgroundHistogramFirstPaint[];

extern const char kHistogramLoadTypeFirstContentfulPaintReload[];
extern const char kHistogramLoadTypeFirstContentfulPaintForwardBack[];
extern const char kHistogramLoadTypeFirstContentfulPaintNewNavigation[];

extern const char kHistogramLoadTypeParseStartReload[];
extern const char kHistogramLoadTypeParseStartForwardBack[];
extern const char kHistogramLoadTypeParseStartNewNavigation[];

extern const char kHistogramFailedProvisionalLoad[];

extern const char kHistogramUserGestureNavigationToForwardBack[];

extern const char kHistogramPageTimingForegroundDuration[];
extern const char kHistogramPageTimingForegroundDurationNoCommit[];

extern const char kHistogramFirstMeaningfulPaintStatus[];

extern const char kHistogramFirstNonScrollInputAfterFirstPaint[];
extern const char kHistogramFirstScrollInputAfterFirstPaint[];

extern const char kHistogramPageLoadTotalBytes[];
extern const char kHistogramPageLoadNetworkBytes[];
extern const char kHistogramPageLoadCacheBytes[];
extern const char kHistogramPageLoadNetworkBytesIncludingHeaders[];
extern const char kHistogramPageLoadUnfinishedBytes[];

extern const char kHistogramPageLoadCpuTotalUsage[];
extern const char kHistogramPageLoadCpuTotalUsageForegrounded[];

extern const char kHistogramLoadTypeTotalBytesForwardBack[];
extern const char kHistogramLoadTypeNetworkBytesForwardBack[];
extern const char kHistogramLoadTypeCacheBytesForwardBack[];

extern const char kHistogramLoadTypeTotalBytesReload[];
extern const char kHistogramLoadTypeNetworkBytesReload[];
extern const char kHistogramLoadTypeCacheBytesReload[];

extern const char kHistogramLoadTypeTotalBytesNewNavigation[];
extern const char kHistogramLoadTypeNetworkBytesNewNavigation[];
extern const char kHistogramLoadTypeCacheBytesNewNavigation[];

extern const char kHistogramTotalCompletedResources[];
extern const char kHistogramNetworkCompletedResources[];
extern const char kHistogramCacheCompletedResources[];

extern const char kHistogramInputToNavigation[];
extern const char kBackgroundHistogramInputToNavigation[];
extern const char kHistogramInputToNavigationLinkClick[];
extern const char kHistogramInputToNavigationOmnibox[];
extern const char kHistogramInputToFirstPaint[];
extern const char kBackgroundHistogramInputToFirstPaint[];
extern const char kHistogramInputToFirstContentfulPaint[];
extern const char kBackgroundHistogramInputToFirstContentfulPaint[];
extern const char kHistogramBackForwardCacheEvent[];

// Navigation metrics from the navigation start.
extern const char
    kHistogramNavigationTimingNavigationStartToFirstRequestStart[];
extern const char
    kHistogramNavigationTimingNavigationStartToFirstResponseStart[];
extern const char
    kHistogramNavigationTimingNavigationStartToFirstLoaderCallback[];
extern const char
    kHistogramNavigationTimingNavigationStartToFinalRequestStart[];
extern const char
    kHistogramNavigationTimingNavigationStartToFinalResponseStart[];
extern const char
    kHistogramNavigationTimingNavigationStartToFinalLoaderCallback[];
extern const char
    kHistogramNavigationTimingNavigationStartToNavigationCommitSent[];

// Navigation metrics between milestones.
extern const char
    kHistogramNavigationTimingFirstRequestStartToFirstResponseStart[];
extern const char
    kHistogramNavigationTimingFirstResponseStartToFirstLoaderCallback[];
extern const char
    kHistogramNavigationTimingFinalRequestStartToFinalResponseStart[];
extern const char
    kHistogramNavigationTimingFinalResponseStartToFinalLoaderCallback[];
extern const char
    kHistogramNavigationTimingFinalLoaderCallbackToNavigationCommitSent[];

// 103 Early Hints metrics for experiment (https://crbug.com/1093693).
extern const char kHistogramEarlyHintsFirstRequestStartToEarlyHints[];
extern const char kHistogramEarlyHintsFinalRequestStartToEarlyHints[];
extern const char kHistogramEarlyHintsEarlyHintsToFinalResponseStart[];

enum FirstMeaningfulPaintStatus {
  FIRST_MEANINGFUL_PAINT_RECORDED,
  FIRST_MEANINGFUL_PAINT_BACKGROUNDED,
  FIRST_MEANINGFUL_PAINT_DID_NOT_REACH_NETWORK_STABLE,
  FIRST_MEANINGFUL_PAINT_USER_INTERACTION_BEFORE_FMP,
  FIRST_MEANINGFUL_PAINT_DID_NOT_REACH_FIRST_CONTENTFUL_PAINT,
  FIRST_MEANINGFUL_PAINT_LAST_ENTRY
};

// Please keep in sync with PageLoadBackForwardCacheEvent in
// tools/metrics/histograms/enums.xml. These values should not be renumbered.
enum class PageLoadBackForwardCacheEvent {
  kEnterBackForwardCache = 0,
  kRestoreFromBackForwardCache = 1,
  kMaxValue = kRestoreFromBackForwardCache,
};

}  // namespace internal

// Observer responsible for recording 'core' UMA page load metrics. Core metrics
// are maintained by loading-dev team, typically the metrics under
// PageLoad.(Document|Paint|Parse)Timing.*.
class UmaPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  UmaPageLoadMetricsObserver();
  ~UmaPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstImagePaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStop(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info)
      override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnUserInput(
      const blink::WebInputEvent& event,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;
  void OnCpuTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::CpuTiming& timing) override;
  ObservePolicy OnEnterBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnRestoreFromBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      content::NavigationHandle* navigation_handle) override;

 private:
  void RecordNavigationTimingHistograms();
  void RecordTimingHistograms(
      const page_load_metrics::mojom::PageLoadTiming& main_frame_timing);
  void RecordByteAndResourceHistograms(
      const page_load_metrics::mojom::PageLoadTiming& timing);
  void RecordCpuUsageHistograms();
  void RecordForegroundDurationHistograms(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      base::TimeTicks app_background_time);

  content::NavigationHandleTiming navigation_handle_timing_;

  ui::PageTransition transition_;
  bool was_no_store_main_resource_;

  // Number of complete resources loaded by the page.
  int num_cache_resources_;
  int num_network_resources_;

  // The number of body (not header) prefilter bytes consumed by completed
  // requests for the page.
  int64_t cache_bytes_;
  int64_t network_bytes_;

  // The number of prefilter bytes consumed by completed and partial network
  // requests for the page.
  int64_t network_bytes_including_headers_;

  // The CPU usage attributed to this page.
  base::TimeDelta total_cpu_usage_;
  base::TimeDelta foreground_cpu_usage_;

  // Size of the redirect chain, which excludes the first URL.
  int redirect_chain_size_;

  // True if we've received a non-scroll input (touch tap or mouse up)
  // after first paint has happened.
  bool received_non_scroll_input_after_first_paint_ = false;

  // True if we've received a scroll input after first paint has happened.
  bool received_scroll_input_after_first_paint_ = false;

  base::TimeTicks first_paint_;

  // Tracks user input clicks for possible click burst.
  page_load_metrics::ClickInputTracker click_tracker_;

  DISALLOW_COPY_AND_ASSIGN(UmaPageLoadMetricsObserver);
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_UMA_PAGE_LOAD_METRICS_OBSERVER_H_
