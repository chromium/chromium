// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_UMA_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_UMA_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "components/page_load_metrics/browser/observers/click_input_tracker.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/navigation_handle_timing.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"

namespace internal {

// NOTE: Some of these histograms are separated into a separate histogram
// specified by the ".Background" suffix. For these events, we put them into the
// background histogram if the web contents was ever in the background from
// navigation start to the event in question.
extern const char kHistogramNumInteractions[];
extern const char
    kHistogramAverageUserInteractionLatencyOverBudgetMaxEventDuration[];
extern const char
    kHistogramSlowUserInteractionLatencyOverBudgetHighPercentile2MaxEventDuration
        [];
extern const char
    kHistogramUserInteractionLatencyHighPercentile2MaxEventDuration[];
extern const char
    kHistogramSumOfUserInteractionLatencyOverBudgetMaxEventDuration[];
extern const char kHistogramWorstUserInteractionLatencyMaxEventDuration[];
extern const char kHistogramInpOffset[];
extern const char kHistogramFirstInputDelay[];
extern const char kHistogramFirstInputTimestamp[];
extern const char kHistogramFirstInputDelay4[];
extern const char kHistogramFirstInputTimestamp4[];
extern const char kHistogramFirstPaint[];
extern const char kHistogramFirstImagePaint[];
extern const char kHistogramDomContentLoaded[];
extern const char kHistogramLoad[];
extern const char kHistogramFirstContentfulPaint[];
extern const char kHistogramLargestContentfulPaint[];
extern const char kHistogramLargestContentfulPaintContentType[];
extern const char kHistogramLargestContentfulPaintMainFrame[];
extern const char kHistogramLargestContentfulPaintMainFrameContentType[];
extern const char kHistogramLargestContentfulPaintCrossSiteSubFrame[];
extern const char
    kHistogramLargestContentfulPaintSetSpeculationRulesPrerender[];
extern const char kHistogramParseBlockedOnScriptLoad[];
extern const char kHistogramParseBlockedOnScriptExecution[];

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

extern const char kHistogramUserGestureNavigationToForwardBack[];

extern const char kHistogramPageTimingForegroundDuration[];
extern const char kHistogramPageTimingForegroundDurationNoCommit[];

extern const char kHistogramCachedResourceLoadTimePrefix[];
extern const char kHistogramCommitSentToFirstSubresourceLoadStart[];
extern const char kHistogramNavigationToFirstSubresourceLoadStart[];
extern const char kHistogramResourceLoadTimePrefix[];
extern const char kHistogramTotalSubresourceLoadTimeAtFirstContentfulPaint[];
extern const char kHistogramFirstEligibleToPaintToFirstPaint[];

extern const char kHistogramPageLoadCpuTotalUsage[];
extern const char kHistogramPageLoadCpuTotalUsageForegrounded[];

extern const char kHistogramInputToNavigation[];
extern const char kBackgroundHistogramInputToNavigation[];
extern const char kHistogramInputToNavigationLinkClick[];
extern const char kHistogramInputToNavigationOmnibox[];
extern const char kHistogramInputToFirstContentfulPaint[];
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

// V8 memory usage metrics.
extern const char kHistogramMemoryMainframe[];
extern const char kHistogramMemorySubframeAggregate[];
extern const char kHistogramMemoryTotal[];

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
// Only pages with web (http/https) schemes are observed.
// UmaFileAndDataPageLoadMetricsObserver records page load metrics for the file
// and data schemes.
class UmaPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  UmaPageLoadMetricsObserver();

  UmaPageLoadMetricsObserver(const UmaPageLoadMetricsObserver&) = delete;
  UmaPageLoadMetricsObserver& operator=(const UmaPageLoadMetricsObserver&) =
      delete;

  ~UmaPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  const char* GetObserverName() const override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
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
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info) override;
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
  void OnV8MemoryChanged(const std::vector<page_load_metrics::MemoryUpdate>&
                             memory_updates) override;

 private:
  // Class to keep track of per-frame memory usage by V8.
  class MemoryUsage {
   public:
    void UpdateUsage(int64_t delta_bytes);

    uint64_t current_bytes_used() { return current_bytes_used_; }
    uint64_t max_bytes_used() { return max_bytes_used_; }

   private:
    uint64_t current_bytes_used_ = 0U;
    uint64_t max_bytes_used_ = 0U;
  };

  void RecordNavigationTimingHistograms();
  void RecordTimingHistograms(
      const page_load_metrics::mojom::PageLoadTiming& main_frame_timing);
  void RecordByteAndResourceHistograms(
      const page_load_metrics::mojom::PageLoadTiming& timing);
  void RecordCpuUsageHistograms();
  void RecordForegroundDurationHistograms(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      base::TimeTicks app_background_time);
  void RecordV8MemoryHistograms();
  void RecordNormalizedResponsivenessMetrics();

  void EmitFCPTraceEvent(base::TimeDelta first_contentful_paint_timing);

  void EmitLCPTraceEvent(base::TimeDelta largest_contentful_paint_timing);

  void EmitInstantTraceEvent(base::TimeDelta duration, const char event_name[]);

  content::NavigationHandleTiming navigation_handle_timing_;

  ui::PageTransition transition_;
  bool was_no_store_main_resource_;

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

  base::TimeTicks first_paint_;

  // Tracks user input clicks for possible click burst.
  page_load_metrics::ClickInputTracker click_tracker_;

  // V8 Memory Usage: whether a memory update was received, the usage of the
  // mainframe, the aggregate usage of all subframes on the page, and the
  // aggregate usage of all frames on the page (including the main frame),
  // respectively.
  bool memory_update_received_ = false;
  MemoryUsage main_frame_memory_usage_;
  MemoryUsage aggregate_subframe_memory_usage_;
  MemoryUsage aggregate_total_memory_usage_;

  bool received_first_subresource_load_ = false;
  base::TimeDelta total_subresource_load_time_;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_UMA_PAGE_LOAD_METRICS_OBSERVER_H_
