// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PAGE_LOAD_METRICS_METRICS_RENDER_FRAME_OBSERVER_H_
#define CHROME_RENDERER_PAGE_LOAD_METRICS_METRICS_RENDER_FRAME_OBSERVER_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "chrome/renderer/page_load_metrics/page_resource_data_use.h"
#include "components/subresource_filter/content/renderer/ad_resource_tracker.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/platform/web_loading_behavior_flag.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace page_load_metrics {

class PageTimingMetricsSender;
class PageTimingSender;

// MetricsRenderFrameObserver observes RenderFrame notifications, and sends page
// load timing information to the browser process over IPC. A
// MetricsRenderFrameObserver is instantiated for each frame (main frames and
// child frames). MetricsRenderFrameObserver dispatches timing and metadata
// updates for main frames, but only metadata updates for child frames.
class MetricsRenderFrameObserver
    : public content::RenderFrameObserver,
      public subresource_filter::AdResourceTracker::Observer {
 public:
  explicit MetricsRenderFrameObserver(content::RenderFrame* render_frame);
  ~MetricsRenderFrameObserver() override;

  // RenderFrameObserver implementation
  void DidChangePerformanceTiming() override;
  void DidObserveLoadingBehavior(
      blink::WebLoadingBehaviorFlag behavior) override;
  void DidObserveNewFeatureUsage(blink::mojom::WebFeature feature) override;
  void DidObserveNewCssPropertyUsage(int css_property,
                                     bool is_animated) override;
  void DidObserveLayoutJank(double jank_fraction) override;
  void DidStartResponse(int request_id,
                        const network::ResourceResponseHead& response_head,
                        content::ResourceType resource_type) override;
  void DidReceiveTransferSizeUpdate(int request_id,
                                    int received_data_length) override;
  void DidCompleteResponse(
      int request_id,
      const network::URLLoaderCompletionStatus& status) override;
  void DidCancelResponse(int request_id) override;
  void DidStartProvisionalLoad(blink::WebDocumentLoader* document_loader,
                               bool is_content_initiated) override;
  void DidFailProvisionalLoad(const blink::WebURLError& error) override;
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void OnDestruct() override;

  // Invoked when a frame is going away. This is our last chance to send IPCs
  // before being destroyed.
  void FrameDetached() override;

  // Set the ad resource tracker that |this| observes.
  void SetAdResourceTracker(
      subresource_filter::AdResourceTracker* ad_resource_tracker);

  // AdResourceTracker implementation
  void OnAdResourceTrackerGoingAway() override;
  void OnAdResourceObserved(int request_id) override;

 private:
  // Updates the metadata for the page resource associated with the given
  // request_id. Removes the request_id from the list of known ads if it is an
  // ad.
  void UpdateResourceMetadata(int request_id);

  void SendMetrics();
  virtual mojom::PageLoadTimingPtr GetTiming() const;
  virtual std::unique_ptr<base::OneShotTimer> CreateTimer();
  virtual std::unique_ptr<PageTimingSender> CreatePageTimingSender();
  virtual bool HasNoRenderFrame() const;

  // Collects the data use of the frame request for a provisional load until the
  // load is committed. We want to collect data use for completed navigations in
  // this class, but the various navigation callbacks do not provide enough data
  // for us to use them for data attribution. Instead, we try to get this
  // information from ongoing resource requests on the previous page (or right
  // before this page loads in a new renderer).
  std::unique_ptr<PageResourceDataUse> provisional_frame_resource_data_use_;
  int provisional_frame_resource_id = 0;

  ScopedObserver<subresource_filter::AdResourceTracker,
                 subresource_filter::AdResourceTracker::Observer>
      scoped_ad_resource_observer_;

  // Set containing all request ids that were reported as ads from the renderer.
  std::set<int> ad_request_ids_;

  // Will be null when we're not actively sending metrics.
  std::unique_ptr<PageTimingMetricsSender> page_timing_metrics_sender_;

  DISALLOW_COPY_AND_ASSIGN(MetricsRenderFrameObserver);
};

}  // namespace page_load_metrics

#endif  // CHROME_RENDERER_PAGE_LOAD_METRICS_METRICS_RENDER_FRAME_OBSERVER_H_
