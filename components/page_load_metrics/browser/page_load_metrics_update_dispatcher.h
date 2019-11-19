// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_UPDATE_DISPATCHER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_UPDATE_DISPATCHER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace page_load_metrics {

class PageLoadMetricsEmbedderInterface;

namespace internal {

// Used to track the status of PageLoadTimings received from the render process.
//
// If you add elements to this enum, make sure you update the enum value in
// histograms.xml. Only add elements to the end to prevent inconsistencies
// between versions.
enum PageLoadTimingStatus {
  // The PageLoadTiming is valid (all data within the PageLoadTiming is
  // consistent with expectations).
  VALID,

  // All remaining status codes are for invalid PageLoadTimings.

  // The PageLoadTiming was empty.
  INVALID_EMPTY_TIMING,

  // The PageLoadTiming had a null navigation_start.
  INVALID_NULL_NAVIGATION_START,

  // Script load or execution durations in the PageLoadTiming were too long.
  INVALID_SCRIPT_LOAD_LONGER_THAN_PARSE,
  INVALID_SCRIPT_EXEC_LONGER_THAN_PARSE,
  INVALID_SCRIPT_LOAD_DOC_WRITE_LONGER_THAN_SCRIPT_LOAD,
  INVALID_SCRIPT_EXEC_DOC_WRITE_LONGER_THAN_SCRIPT_EXEC,

  // The order of two events in the PageLoadTiming was invalid. Either the first
  // wasn't present when the second was present, or the second was reported as
  // happening before the first.
  INVALID_ORDER_RESPONSE_START_PARSE_START,
  INVALID_ORDER_PARSE_START_PARSE_STOP,
  INVALID_ORDER_PARSE_STOP_DOM_CONTENT_LOADED,
  INVALID_ORDER_DOM_CONTENT_LOADED_LOAD,
  INVALID_ORDER_PARSE_START_FIRST_LAYOUT,
  INVALID_ORDER_FIRST_LAYOUT_FIRST_PAINT,
  // Deprecated but not removing because it would affect histogram enumeration.
  INVALID_ORDER_FIRST_PAINT_FIRST_TEXT_PAINT,
  INVALID_ORDER_FIRST_PAINT_FIRST_IMAGE_PAINT,
  INVALID_ORDER_FIRST_PAINT_FIRST_CONTENTFUL_PAINT,
  INVALID_ORDER_FIRST_PAINT_FIRST_MEANINGFUL_PAINT,
  INVALID_ORDER_FIRST_MEANINGFUL_PAINT_PAGE_INTERACTIVE,

  // We received a first input delay without a first input timestamp.
  INVALID_NULL_FIRST_INPUT_TIMESTAMP,
  // We received a first input timestamp without a first input delay.
  INVALID_NULL_FIRST_INPUT_DELAY,

  // We received a longest input delay without a longest input timestamp.
  INVALID_NULL_LONGEST_INPUT_TIMESTAMP,
  // We received a longest input timestamp without a longest input delay.
  INVALID_NULL_LONGEST_INPUT_DELAY,

  // Longest input delay cannot happen before first input delay.
  INVALID_LONGEST_INPUT_TIMESTAMP_LESS_THAN_FIRST_INPUT_TIMESTAMP,

  // Longest input delay cannot be less than first input delay.
  INVALID_LONGEST_INPUT_DELAY_LESS_THAN_FIRST_INPUT_DELAY,

  // New values should be added before this final entry.
  LAST_PAGE_LOAD_TIMING_STATUS
};

extern const char kPageLoadTimingStatus[];
extern const char kHistogramOutOfOrderTiming[];
extern const char kHistogramOutOfOrderTimingBuffered[];

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
    virtual ~Client() {}

    virtual void OnTimingChanged() = 0;
    virtual void OnSubFrameTimingChanged(
        content::RenderFrameHost* rfh,
        const mojom::PageLoadTiming& timing) = 0;
    virtual void OnMainFrameMetadataChanged() = 0;
    virtual void OnSubframeMetadataChanged(
        content::RenderFrameHost* rfh,
        const mojom::PageLoadMetadata& metadata) = 0;
    virtual void OnSubFrameRenderDataChanged(
        content::RenderFrameHost* rfh,
        const mojom::FrameRenderDataUpdate& render_data) = 0;
    virtual void UpdateFeaturesUsage(
        content::RenderFrameHost* rfh,
        const mojom::PageLoadFeatures& new_features) = 0;
    virtual void UpdateResourceDataUse(
        content::RenderFrameHost* rfh,
        const std::vector<mojom::ResourceDataUpdatePtr>& resources) = 0;
    virtual void UpdateFrameCpuTiming(content::RenderFrameHost* rfh,
                                      const mojom::CpuTiming& timing) = 0;
    virtual void OnNewDeferredResourceCounts(
        const mojom::DeferredResourceCounts& new_deferred_resource_data) = 0;
  };

  // The |client| instance must outlive this object.
  PageLoadMetricsUpdateDispatcher(
      Client* client,
      content::NavigationHandle* navigation_handle,
      PageLoadMetricsEmbedderInterface* embedder_interface);
  ~PageLoadMetricsUpdateDispatcher();

  void UpdateMetrics(
      content::RenderFrameHost* render_frame_host,
      mojom::PageLoadTimingPtr new_timing,
      mojom::PageLoadMetadataPtr new_metadata,
      mojom::PageLoadFeaturesPtr new_features,
      const std::vector<mojom::ResourceDataUpdatePtr>& resources,
      mojom::FrameRenderDataUpdatePtr render_data,
      mojom::CpuTimingPtr new_cpu_timing,
      mojom::DeferredResourceCountsPtr new_deferred_resource_data);

  // This method is only intended to be called for PageLoadFeatures being
  // recorded directly from the browser process. Features coming from the
  // renderer process should use the main flow into |UpdateMetrics|.
  void UpdateFeatures(content::RenderFrameHost* render_frame_host,
                      const mojom::PageLoadFeatures& new_features);

  void DidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle);

  void ShutDown();

  const mojom::PageLoadTiming& timing() const {
    return *(current_merged_page_timing_.get());
  }

  const mojom::PageLoadMetadata& main_frame_metadata() const {
    return *(main_frame_metadata_.get());
  }
  const mojom::PageLoadMetadata& subframe_metadata() const {
    return *(subframe_metadata_.get());
  }
  const PageRenderData& page_render_data() const { return page_render_data_; }
  const PageRenderData& main_frame_render_data() const {
    return main_frame_render_data_;
  }

 private:
  using FrameTreeNodeId = int;

  void UpdateMainFrameTiming(mojom::PageLoadTimingPtr new_timing);
  void UpdateSubFrameTiming(content::RenderFrameHost* render_frame_host,
                            mojom::PageLoadTimingPtr new_timing);
  void UpdateFrameCpuTiming(content::RenderFrameHost* render_frame_host,
                            mojom::CpuTimingPtr new_timing);

  void UpdateMainFrameMetadata(mojom::PageLoadMetadataPtr new_metadata);
  void UpdateSubFrameMetadata(content::RenderFrameHost* render_frame_host,
                              mojom::PageLoadMetadataPtr subframe_metadata);

  void UpdatePageRenderData(const mojom::FrameRenderDataUpdate& render_data);
  void UpdateMainFrameRenderData(
      const mojom::FrameRenderDataUpdate& render_data);
  void OnSubFrameRenderDataChanged(
      content::RenderFrameHost* render_frame_host,
      const mojom::FrameRenderDataUpdate& render_data);

  void MaybeDispatchTimingUpdates(bool did_merge_new_timing_value);
  void DispatchTimingUpdates();

  // The client is guaranteed to outlive this object.
  Client* const client_;

  // Interface to chrome features. Must outlive the class.
  PageLoadMetricsEmbedderInterface* const embedder_interface_;

  std::unique_ptr<base::OneShotTimer> timer_;

  // Time the navigation for this page load was initiated.
  const base::TimeTicks navigation_start_;

  // PageLoadTiming for the currently tracked page. The fields in |paint_timing|
  // are merged across all frames in the document. All other fields are from the
  // main frame document. |current_merged_page_timing_| contains the most recent
  // valid page load timing data, while pending_merged_page_timing_ contains
  // pending updates received since |current_merged_page_timing_| was last
  // dispatched to the client. pending_merged_page_timing_ will be copied to
  // |current_merged_page_timing_| once it is valid, at the time the
  // Client::OnTimingChanged callback is invoked.
  mojom::PageLoadTimingPtr current_merged_page_timing_;
  mojom::PageLoadTimingPtr pending_merged_page_timing_;

  mojom::PageLoadMetadataPtr main_frame_metadata_;
  mojom::PageLoadMetadataPtr subframe_metadata_;

  PageRenderData page_render_data_;
  PageRenderData main_frame_render_data_;

  // Navigation start offsets for the most recently committed document in each
  // frame.
  std::map<FrameTreeNodeId, base::TimeDelta> subframe_navigation_start_offset_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsUpdateDispatcher);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_UPDATE_DISPATCHER_H_
