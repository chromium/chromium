// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/observers/click_input_tracker.h"
#include "components/page_load_metrics/browser/observers/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace internal {

// NOTE: Some of these histograms are separated into a separate histogram
// specified by the ".Background" suffix. For these events, we put them into the
// background histogram if the web contents was ever in the background from
// navigation start to the event in question.
extern const char kHistogramFirstLayout[];
extern const char kHistogramFirstInputDelay[];
extern const char kHistogramFirstInputTimestamp[];
extern const char kHistogramFirstInputDelaySkipFilteringComparison[];
extern const char kHistogramFirstInputTimestampSkipFilteringComparison[];
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
extern const char kHistogramLargestImagePaint[];
extern const char kHistogramLargestTextPaint[];
extern const char kHistogramLargestContentfulPaint[];
extern const char kHistogramLargestContentfulPaintContentType[];
extern const char kHistogramLargestContentfulPaintMainFrame[];
extern const char kHistogramLargestContentfulPaintMainFrameContentType[];
extern const char kHistogramTimeToInteractive[];
extern const char kHistogramParseDuration[];
extern const char kHistogramParseBlockedOnScriptLoad[];
extern const char kHistogramParseBlockedOnScriptExecution[];
extern const char kHistogramParseStartToFirstMeaningfulPaint[];

extern const char kBackgroundHistogramFirstLayout[];
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

extern const char kHistogramForegroundToFirstMeaningfulPaint[];

extern const char kHistogramFirstMeaningfulPaintStatus[];
extern const char kHistogramTimeToInteractiveStatus[];

extern const char kHistogramFirstNonScrollInputAfterFirstPaint[];
extern const char kHistogramFirstScrollInputAfterFirstPaint[];

extern const char kHistogramPageLoadTotalBytes[];
extern const char kHistogramPageLoadNetworkBytes[];
extern const char kHistogramPageLoadCacheBytes[];
extern const char kHistogramPageLoadNetworkBytesIncludingHeaders[];
extern const char kHistogramPageLoadUnfinishedBytes[];

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

enum FirstMeaningfulPaintStatus {
  FIRST_MEANINGFUL_PAINT_RECORDED,
  FIRST_MEANINGFUL_PAINT_BACKGROUNDED,
  FIRST_MEANINGFUL_PAINT_DID_NOT_REACH_NETWORK_STABLE,
  FIRST_MEANINGFUL_PAINT_USER_INTERACTION_BEFORE_FMP,
  FIRST_MEANINGFUL_PAINT_DID_NOT_REACH_FIRST_CONTENTFUL_PAINT,
  FIRST_MEANINGFUL_PAINT_LAST_ENTRY
};

// Different events can prevent us from recording a successful time to
// interactive value, and sometimes several of these failure events can happen
// simultaneously. In the case of multiple invalidating events, we record the
// failure reason in decreasing order of priority of the following:
//
// 1. Did not reach First Meaningful Paint
// 2. Did not reach quiescence
// 3. Page was not in foreground until TTI was detected.
// 4. There was a non-mouse-move user input before interactive time.
//
// Table of conditions and the TTI Status recorded for each case:
// FMP Reached | Quiesence Reached | Always Foreground | No User Input | Status
// True        | True              | True              | True          | 0
// True        | True              | True              | False         | 2
// True        | True              | False             | *             | 1
// True        | False             | *                 | *             | 3
// False       | *                 | *                 | *             | 4
enum TimeToInteractiveStatus {
  // Time to Interactive recorded successfully.
  TIME_TO_INTERACTIVE_RECORDED = 0,

  // Main thread and network quiescence reached, but the user backgrounded the
  // page at least once before reaching quiescence.
  TIME_TO_INTERACTIVE_BACKGROUNDED = 1,

  // Main thread and network quiescence reached, but there was a non-mouse-move
  // user input that hit the renderer main thread between navigation start and
  // interactive time, so the detected interactive time is inaccurate. Note that
  // Time to Interactive is not invalidated if the user input is after
  // interactive time, but before quiescence windows are detected. User input
  // invalidation has less priority than backgrounding - if there was an input
  // event before reaching interactive, but the page was backgrounded before
  // reaching interactive detection, the status is recorded as backgrounded
  // instead of user-interaction-before-interactive.
  TIME_TO_INTERACTIVE_USER_INTERACTION_BEFORE_INTERACTIVE = 2,

  // User left page before main thread and network quiescence, but after First
  // Meaningful Paint.
  TIME_TO_INTERACTIVE_DID_NOT_REACH_QUIESCENCE = 3,

  // User left page before First Meaningful Paint happened, but after First
  // Paint. This status will also be recorded if the first meaningful paint was
  // reached on the renderer, but invalidated there due to user input. Input
  // invalided First Meaningful Paint values do not reach the browser.
  TIME_TO_INTERACTIVE_DID_NOT_REACH_FIRST_MEANINGFUL_PAINT = 4,

  TIME_TO_INTERACTIVE_LAST_ENTRY
};

}  // namespace internal

// Observer responsible for recording 'core' page load metrics. Core metrics are
// maintained by loading-dev team, typically the metrics under
// PageLoad.(Document|Paint|Parse)Timing.*.
class CorePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  CorePageLoadMetricsObserver();
  ~CorePageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstLayout(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstImagePaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnPageInteractive(
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

  void OnTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  void TrackPossibleClickBurst(const blink::WebInputEvent& event);
  void RecordTimingHistograms(
      const page_load_metrics::mojom::PageLoadTiming& main_frame_timing);
  void RecordByteAndResourceHistograms(
      const page_load_metrics::mojom::PageLoadTiming& timing);
  void RecordForegroundDurationHistograms(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      base::TimeTicks app_background_time);

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

  // Size of the redirect chain, which excludes the first URL.
  int redirect_chain_size_;

  // True if we've received a non-scroll input (touch tap or mouse up)
  // after first paint has happened.
  bool received_non_scroll_input_after_first_paint_ = false;

  // True if we've received a scroll input after first paint has happened.
  bool received_scroll_input_after_first_paint_ = false;

  base::TimeTicks first_paint_;

  page_load_metrics::LargestContentfulPaintHandler
      largest_contentful_paint_handler_;

  // Tracks user input clicks for possible click burst.
  page_load_metrics::ClickInputTracker click_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CorePageLoadMetricsObserver);
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_PAGE_LOAD_METRICS_OBSERVER_H_
