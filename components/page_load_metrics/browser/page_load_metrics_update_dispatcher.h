// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_UPDATE_DISPATCHER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_UPDATE_DISPATCHER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/page_load_metrics/browser/layout_shift_normalization.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/responsiveness_metrics_normalization.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace page_load_metrics {

class PageLoadMetricsEmbedderInterface;

namespace internal {

enum class PageLoadTrackerPageType;

// Used to track the status of PageLoadTimings received from the render process.
//
// These values are recorded in histograms. Entries should not be renumbered
// and numeric values should never be reused.
//
// If you add elements to this enum, make sure you update the enum value in
// histograms.xml. Only add elements to the end to prevent inconsistencies
// between versions.
// LINT.IfChange(PageLoadTimingStatus)
enum PageLoadTimingStatus {
  // The PageLoadTiming is valid (all data within the PageLoadTiming is
  // consistent with expectations).
  VALID = 0,

  // All remaining status codes are for invalid PageLoadTimings.

  // The PageLoadTiming was empty.
  INVALID_EMPTY_TIMING = 1,

  // The PageLoadTiming had a null navigation_start.
  INVALID_NULL_NAVIGATION_START = 2,

  // Script load or execution durations in the PageLoadTiming were too long.
  INVALID_SCRIPT_LOAD_LONGER_THAN_PARSE = 3,
  INVALID_SCRIPT_EXEC_LONGER_THAN_PARSE = 4,
  INVALID_SCRIPT_LOAD_DOC_WRITE_LONGER_THAN_SCRIPT_LOAD = 5,
  INVALID_SCRIPT_EXEC_DOC_WRITE_LONGER_THAN_SCRIPT_EXEC = 6,

  // The order of two events in the PageLoadTiming was invalid. Either the first
  // wasn't present when the second was present, or the second was reported as
  // happening before the first.
  INVALID_ORDER_RESPONSE_START_PARSE_START = 7,
  INVALID_ORDER_PARSE_START_PARSE_STOP = 8,
  INVALID_ORDER_PARSE_STOP_DOM_CONTENT_LOADED = 9,
  INVALID_ORDER_DOM_CONTENT_LOADED_LOAD = 10,
  INVALID_ORDER_PARSE_START_FIRST_PAINT = 11,
  // Deprecated but not removing because it would affect histogram enumeration.
  INVALID_ORDER_FIRST_PAINT_FIRST_TEXT_PAINT = 12,
  INVALID_ORDER_FIRST_PAINT_FIRST_IMAGE_PAINT = 13,
  INVALID_ORDER_FIRST_PAINT_FIRST_CONTENTFUL_PAINT = 14,
  INVALID_ORDER_FIRST_PAINT_FIRST_MEANINGFUL_PAINT = 15,
  // Deprecated but not removing because it would affect histogram enumeration.
  INVALID_ORDER_FIRST_MEANINGFUL_PAINT_PAGE_INTERACTIVE = 16,

  // We received a first input delay without a first input timestamp.
  INVALID_NULL_FIRST_INPUT_TIMESTAMP = 17,
  // We received a first input timestamp without a first input delay.
  INVALID_NULL_FIRST_INPUT_DELAY = 18,

  // We received a longest input delay without a longest input timestamp.
  INVALID_NULL_LONGEST_INPUT_TIMESTAMP = 19,
  // We received a longest input timestamp without a longest input delay.
  INVALID_NULL_LONGEST_INPUT_DELAY = 20,

  // We received a first scroll delay without a first scroll timestamp.
  INVALID_NULL_FIRST_SCROLL_TIMESTAMP = 21,
  // We received a first scroll timestamp without a first scroll delay.
  INVALID_NULL_FIRST_SCROLL_DELAY = 22,

  // Longest input delay cannot happen before first input delay.
  INVALID_LONGEST_INPUT_TIMESTAMP_LESS_THAN_FIRST_INPUT_TIMESTAMP = 23,

  // Longest input delay cannot be less than first input delay.
  INVALID_LONGEST_INPUT_DELAY_LESS_THAN_FIRST_INPUT_DELAY = 24,

  // Deprecated but not removing because it would affect histogram enumeration.
  INVALID_ORDER_PARSE_START_ACTIVATION_START = 25,
  INVALID_ORDER_ACTIVATION_START_FIRST_PAINT = 26,

  // New values should be added before this final entry.
  LAST_PAGE_LOAD_TIMING_STATUS,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:PageLoadTimingStatus)

extern const char kPageLoadTimingStatus[];

}  // namespace internal

// PageLoadMetricsUpdateDispatcher manages updates to page load metrics data,
// and dispatches them to the Client. PageLoadMetricsUpdateDispatcher may delay
// dispatching metrics updates to the Client in cases where metrics state hasn't
// stabilized.
class PageLoadMetricsUpdateDispatcher {
 public:
  // The Client class is updated when metrics managed by the dispatcher have
  // changed. Typically it owns the dispatcher.
  class Client {
   public:
    virtual ~Client() = default;

    virtual PrerenderingState GetPrerenderingState() const = 0;
    virtual bool IsPageMainFrame(content::RenderFrameHost* rfh) const = 0;
    virtual void OnTimingChanged() = 0;
    virtual void OnPageInputTimingChanged(uint64_t num_interactions) = 0;
    virtual void OnSubFrameTimingChanged(
        content::RenderFrameHost* rfh,
        const mojom::PageLoadTiming& timing) = 0;
    virtual void OnMainFrameMetadataChanged() = 0;
    virtual void OnSubframeMetadataChanged(
        content::RenderFrameHost* rfh,
        const mojom::FrameMetadata& metadata) = 0;
    virtual void OnSubFrameInputTimingChanged(
        content::RenderFrameHost* rfh,
        const mojom::InputTiming& input_timing_delta) = 0;
    virtual void OnPageRenderDataChanged(
        const mojom::FrameRenderDataUpdate& render_data,
        bool is_main_frame) = 0;
    virtual void OnSubFrameRenderDataChanged(
        content::RenderFrameHost* rfh,
        const mojom::FrameRenderDataUpdate& render_data) = 0;
    virtual void OnSoftNavigationChanged(
        const mojom::SoftNavigationMetrics& soft_navigation_metrics) = 0;
    virtual void UpdateFeaturesUsage(
        content::RenderFrameHost* rfh,
        const std::vector<blink::UseCounterFeature>& new_features) = 0;
    virtual void UpdateResourceDataUse(
        content::RenderFrameHost* rfh,
        const std::vector<mojom::ResourceDataUpdatePtr>& resources) = 0;
    virtual void UpdateFrameCpuTiming(content::RenderFrameHost* rfh,
                                      const mojom::CpuTiming& timing) = 0;
    virtual void OnMainFrameIntersectionRectChanged(
        content::RenderFrameHost* rfh,
        const gfx::Rect& main_frame_intersection_rect) = 0;
    virtual void OnMainFrameViewportRectChanged(
        const gfx::Rect& main_frame_viewport_rect) = 0;
    virtual void OnMainFrameImageAdRectsChanged(
        const base::flat_map<int, gfx::Rect>& main_frame_image_ad_rects) = 0;
    virtual void SetUpSharedMemoryForSmoothness(
        base::ReadOnlySharedMemoryRegion shared_memory) = 0;
  };

  // The |client| instance must outlive this object.
  PageLoadMetricsUpdateDispatcher(
      Client* client,
      content::NavigationHandle* navigation_handle,
      PageLoadMetricsEmbedderInterface* embedder_interface);

  PageLoadMetricsUpdateDispatcher(const PageLoadMetricsUpdateDispatcher&) =
      delete;
  PageLoadMetricsUpdateDispatcher& operator=(
      const PageLoadMetricsUpdateDispatcher&) = delete;

  ~PageLoadMetricsUpdateDispatcher();

  void UpdateMetrics(content::RenderFrameHost* render_frame_host,
                     mojom::PageLoadTimingPtr new_timing,
                     mojom::FrameMetadataPtr new_metadata,
                     const std::vector<blink::UseCounterFeature>& new_features,
                     const std::vector<mojom::ResourceDataUpdatePtr>& resources,
                     mojom::FrameRenderDataUpdatePtr render_data,
                     mojom::CpuTimingPtr new_cpu_timing,
                     mojom::InputTimingPtr input_timing_delta,
                     const std::optional<blink::SubresourceLoadMetrics>&
                         subresource_load_metrics,
                     mojom::SoftNavigationMetricsPtr soft_navigation_metrics,
                     internal::PageLoadTrackerPageType page_type);

  void SetUpSharedMemoryForSmoothness(
      content::RenderFrameHost* render_frame_host,
      base::ReadOnlySharedMemoryRegion shared_memory);

  // This method is only intended to be called for PageLoadFeatures being
  // recorded directly from the browser process. Features coming from the
  // renderer process should use the main flow into |UpdateMetrics|.
  void UpdateFeatures(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::UseCounterFeature>& new_features);

  void DidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle);

  void OnSubFrameDeleted(content::FrameTreeNodeId frame_tree_node_id);

  void ShutDown();

  const mojom::PageLoadTiming& timing() const {
    return *(current_merged_page_timing_.get());
  }

  const mojom::FrameMetadata& main_frame_metadata() const {
    return *(main_frame_metadata_.get());
  }
  const mojom::FrameMetadata& subframe_metadata() const {
    return *(subframe_metadata_.get());
  }
  const PageRenderData& page_render_data() const { return page_render_data_; }
  const NormalizedCLSData& normalized_cls_data(
      PageLoadMetricsObserverDelegate::BfcacheStrategy bfcache_strategy) const {
    return bfcache_strategy ==
                   PageLoadMetricsObserverDelegate::BfcacheStrategy::RESET
               ? layout_shift_normalization_for_bfcache_.normalized_cls_data()
               : layout_shift_normalization_.normalized_cls_data();
  }
  const ResponsivenessMetricsNormalization&
  responsiveness_metrics_normalization() const {
    return responsiveness_metrics_normalization_;
  }

  const ResponsivenessMetricsNormalization&
  soft_navigation_interval_responsiveness_metrics_normalization() const {
    return soft_navigation_interval_responsiveness_metrics_normalization_;
  }

  const NormalizedCLSData& soft_navigation_interval_normalized_layout_shift()
      const {
    return soft_nav_interval_layout_shift_normalization_.normalized_cls_data();
  }

  void ResetSoftNavigationIntervalResponsivenessMetricsNormalization() {
    soft_navigation_interval_responsiveness_metrics_normalization_
        .ClearAllUserInteractionLatencies();
  }

  const PageRenderData& main_frame_render_data() const {
    return main_frame_render_data_;
  }
  const mojom::InputTiming& page_input_timing() const {
    return *page_input_timing_;
  }
  const std::optional<blink::SubresourceLoadMetrics>& subresource_load_metrics()
      const {
    return subresource_load_metrics_;
  }
  void UpdateResponsivenessMetricsNormalizationForBfcache() {
    responsiveness_metrics_normalization_.ClearAllUserInteractionLatencies();
  }
  void UpdateLayoutShiftNormalizationForBfcache() {
    cumulative_layout_shift_score_for_bfcache_ =
        page_render_data_.layout_shift_score;
    layout_shift_normalization_for_bfcache_.ClearAllLayoutShifts();
  }

  void ResetSoftNavigationIntervalLayoutShift() {
    soft_nav_interval_render_data_.layout_shift_score = 0;
    soft_nav_interval_render_data_.layout_shift_score_before_input_or_scroll =
        0;
    soft_nav_interval_layout_shift_normalization_.ClearAllLayoutShifts();
  }

  // Ensures all pending updates will get dispatched.
  void FlushPendingTimingUpdates();

 private:
  void UpdateMainFrameTiming(mojom::PageLoadTimingPtr new_timing,
                             internal::PageLoadTrackerPageType page_type);
  void UpdateSubFrameTiming(content::RenderFrameHost* render_frame_host,
                            mojom::PageLoadTimingPtr new_timing);
  void UpdateFrameCpuTiming(content::RenderFrameHost* render_frame_host,
                            mojom::CpuTimingPtr new_timing);
  void UpdateSubFrameInputTiming(content::RenderFrameHost* render_frame_host,
                                 const mojom::InputTiming& input_timing_delta);

  void UpdateMainFrameMetadata(content::RenderFrameHost* render_frame_host,
                               mojom::FrameMetadataPtr new_metadata);
  void UpdateSubFrameMetadata(content::RenderFrameHost* render_frame_host,
                              mojom::FrameMetadataPtr subframe_metadata);

  void UpdateMainFrameSubresourceLoadMetrics(
      const blink::SubresourceLoadMetrics& subresource_load_metrics);

  void UpdateSoftNavigation(
      const mojom::SoftNavigationMetrics& soft_navigation_metrics);

  void UpdateSoftNavigationIntervalResponsivenessMetrics(
      const mojom::InputTiming& input_timing_delta);

  void UpdateSoftNavigationIntervalLayoutShift(
      const mojom::FrameRenderDataUpdate& render_data);

  void UpdatePageInputTiming(const mojom::InputTiming& input_timing_delta);

  void MaybeUpdateMainFrameIntersectionRect(
      content::RenderFrameHost* render_frame_host,
      const mojom::FrameMetadataPtr& frame_metadata);
  void MaybeUpdateMainFrameViewportRect(
      const mojom::FrameMetadataPtr& frame_metadata);

  void UpdatePageRenderData(const mojom::FrameRenderDataUpdate& render_data,
                            bool is_main_frame);
  void UpdateMainFrameRenderData(
      const mojom::FrameRenderDataUpdate& render_data);
  void OnSubFrameRenderDataChanged(
      content::RenderFrameHost* render_frame_host,
      const mojom::FrameRenderDataUpdate& render_data);

  void MaybeDispatchTimingUpdates(bool did_merge_new_timing_value);
  void DispatchTimingUpdates();

  void UpdateHasSeenInputOrScroll(const mojom::PageLoadTiming& new_timing);

  // The client is guaranteed to outlive this object.
  const raw_ptr<Client> client_;

  // Interface to chrome features. Must outlive the class.
  const raw_ptr<PageLoadMetricsEmbedderInterface> embedder_interface_;

  std::unique_ptr<base::OneShotTimer> timer_;

  // Time the navigation for this page load was initiated.
  const base::TimeTicks navigation_start_;

  // PageLoadTiming for the currently tracked page. Some fields, such as FCP,
  // are merged across all frames in the document, while other fields are from
  // the main frame only (see PageLoadTimingMerger).
  //
  // |current_merged_page_timing_| contains the most recent valid timing data,
  // while |pending_merged_page_timing_| contains pending updates received since
  // |current_merged_page_timing_| was last dispatched to the client (see
  // DispatchTimingUpdates, which invokes the Client::OnTimingChanged callback).
  //
  mojom::PageLoadTimingPtr current_merged_page_timing_;
  mojom::PageLoadTimingPtr pending_merged_page_timing_;

  // TODO(crbug.com/40677945): Replace aggregate frame metadata with a separate
  // struct instead of using mojo.
  mojom::FrameMetadataPtr main_frame_metadata_;
  mojom::FrameMetadataPtr subframe_metadata_;

  // InputTiming data accumulated across all frames.
  mojom::InputTimingPtr page_input_timing_;

  // SubresourceLoadMetrics for the main frame.
  std::optional<blink::SubresourceLoadMetrics> subresource_load_metrics_;

  // True if this page load started in prerender.
  const bool is_prerendered_page_load_;

  // In general, page_render_data_ contains combined data across all frames on
  // the page, while main_frame_render_data_ contains data specific to the main
  // frame.
  //
  // The layout_shift_score_before_input_or_scroll field in page_render_data_
  // represents CLS across all frames (with subframe weighting), measured until
  // first input/scroll in any frame (including an OOPIF).
  //
  // The main frame layout_shift_score_before_input_or_scroll represents CLS
  // occurring within the main frame, measured until the first input/scroll seen
  // by the main frame (or an input sent to a same-site subframe, due to
  // crbug.com/1136207).
  //
  PageRenderData page_render_data_;
  PageRenderData main_frame_render_data_;

  PageRenderData soft_nav_interval_render_data_;

  // The last main frame intersection rects dispatched to page load metrics
  // observers.
  std::map<content::FrameTreeNodeId, gfx::Rect> main_frame_intersection_rects_;

  // The last main frame viewport rect dispatched to page load metrics
  // observers.
  std::optional<gfx::Rect> main_frame_viewport_rect_;

  LayoutShiftNormalization layout_shift_normalization_;
  LayoutShiftNormalization soft_nav_interval_layout_shift_normalization_;

  // Layout shift normalization data for bfcache which needs to be reset each
  // time the page enters the BackForward cache.
  LayoutShiftNormalization layout_shift_normalization_for_bfcache_;
  float cumulative_layout_shift_score_for_bfcache_ = 0.0;

  // Navigation start offsets for the most recently committed document in each
  // frame.
  std::map<content::FrameTreeNodeId, base::TimeDelta>
      subframe_navigation_start_offset_;

  // Whether we have seen an input or scroll event in any frame. This comes to
  // us via PaintTimingDetector::OnInputOrScroll, which triggers on user scrolls
  // and most input types (but not mousemove or pinch zoom). More comments in
  // UpdateHasSeenInputOrScroll.
  bool has_seen_input_or_scroll_ = false;

  // Where we receive user interaction latencies from all renderer frames and
  // calculate a few normalized responsiveness metrics. It will be reset every
  // time the page enters bfcache.
  ResponsivenessMetricsNormalization responsiveness_metrics_normalization_;

  // Keeps track of user interaction latencies on main frame for soft
  // navigation intervals. A soft navigation interval is either the
  // interval from page load start to 1st soft navigation, or an interval
  // between 2 soft navigations, or the interval from the last soft navigation
  // to the page load end.
  ResponsivenessMetricsNormalization
      soft_navigation_interval_responsiveness_metrics_normalization_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_UPDATE_DISPATCHER_H_
