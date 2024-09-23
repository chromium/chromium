// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_METRICS_RENDER_FRAME_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_METRICS_RENDER_FRAME_OBSERVER_H_

#include <memory>
#include <optional>
#include <set>

#include "base/scoped_observation.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/renderer/page_resource_data_use.h"
#include "components/page_load_metrics/renderer/page_timing_metadata_recorder.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/public/common/subresource_load_metrics.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/web/web_local_frame_observer.h"

class GURL;

namespace base {
class OneShotTimer;
}  // namespace base

namespace blink {
struct JavaScriptFrameworkDetectionResult;
struct SoftNavigationMetrics;
}  // namespace blink

namespace page_load_metrics {

namespace internal {
const char kPageLoadInternalSoftNavigationFromStartInvalidTiming[] =
    "PageLoad.Internal.SoftNavigationFromStartInvalidTiming";

// These values are recorded into a UMA histogram as scenarios where the start
// time of soft navigation ends up being 0. These entries
// should not be renumbered and the numeric values should not be reused. These
// entries should be kept in sync with the definition in
// tools/metrics/histograms/enums.xml
// TODO(crbug.com/40074158): Remove the code here and related code once the bug
// is resolved.
enum class SoftNavigationFromStartInvalidTimingReasons {
  kSoftNavStartTimeIsZeroAndLtNavStart = 0,
  kSoftNavStartTimeIsZeroAndEqNavStart = 1,
  kSoftNavStartTimeIsNonZeroAndEqNavStart = 2,
  kSoftNavStartTimeIsNonZeroAndLtNavStart = 3,
  kMaxValue = kSoftNavStartTimeIsNonZeroAndLtNavStart,
};

void RecordUmaForkPageLoadInternalSoftNavigationFromStartInvalidTiming(
    base::TimeDelta start_time_relative_to_reference,
    double nav_start_to_reference);

}  // namespace internal

class PageTimingMetricsSender;
class PageTimingSender;

// MetricsRenderFrameObserver observes RenderFrame notifications, and sends page
// load timing information to the browser process over IPC. A
// MetricsRenderFrameObserver is instantiated for each frame (main frames and
// child frames). MetricsRenderFrameObserver dispatches timing and metadata
// updates for main frames, but only metadata updates for child frames.
class MetricsRenderFrameObserver : public content::RenderFrameObserver,
                                   public blink::WebLocalFrameObserver {
 public:
  explicit MetricsRenderFrameObserver(content::RenderFrame* render_frame);

  MetricsRenderFrameObserver(const MetricsRenderFrameObserver&) = delete;
  MetricsRenderFrameObserver& operator=(const MetricsRenderFrameObserver&) =
      delete;

  ~MetricsRenderFrameObserver() override;

  // RenderFrameObserver implementation
  void DidChangePerformanceTiming() override;
  void DidObserveUserInteraction(base::TimeTicks max_event_start,
                                 base::TimeTicks max_event_queued_main_thread,
                                 base::TimeTicks max_event_commit_finish,
                                 base::TimeTicks max_event_end,
                                 blink::UserInteractionType interaction_type,
                                 uint64_t interaction_offset) override;
  void DidChangeCpuTiming(base::TimeDelta time) override;
  void DidObserveLoadingBehavior(blink::LoadingBehaviorFlag behavior) override;
  void DidObserveJavaScriptFrameworks(
      const blink::JavaScriptFrameworkDetectionResult&) override;
  void DidObserveSubresourceLoad(
      const blink::SubresourceLoadMetrics& subresource_load_metrics) override;
  void DidObserveNewFeatureUsage(
      const blink::UseCounterFeature& feature) override;
  void DidObserveSoftNavigation(blink::SoftNavigationMetrics metrics) override;
  void DidObserveLayoutShift(double score, bool after_input_or_scroll) override;
  void DidStartResponse(const url::SchemeHostPort& final_response_url,
                        int request_id,
                        const network::mojom::URLResponseHead& response_head,
                        network::mojom::RequestDestination request_destination,
                        bool is_ad_resource) override;
  void DidReceiveTransferSizeUpdate(int request_id,
                                    int received_data_length) override;
  void DidCompleteResponse(
      int request_id,
      const network::URLLoaderCompletionStatus& status) override;
  void DidCancelResponse(int request_id) override;
  void DidLoadResourceFromMemoryCache(const GURL& response_url,
                                      int request_id,
                                      int64_t encoded_body_length,
                                      const std::string& mime_type,
                                      bool from_archive) override;
  void DidStartNavigation(
      const GURL& url,
      std::optional<blink::WebNavigationType> navigation_type) override;
  void DidSetPageLifecycleState(bool restoring_from_bfcache) override;

  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidFailProvisionalLoad() override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidCreateDocumentElement() override;
  void OnDestruct() override;

  // Invoked when a frame is going away. This is our last chance to send IPCs
  // before being destroyed.
  void WillDetach(blink::DetachReason detach_reason) override;

  void OnMainFrameIntersectionChanged(
      const gfx::Rect& main_frame_intersection_rect) override;
  void OnMainFrameViewportRectangleChanged(
      const gfx::Rect& main_frame_viewport_rect) override;
  void OnMainFrameImageAdRectangleChanged(
      int element_id,
      const gfx::Rect& image_ad_rect) override;

  // blink::WebLocalFrameObserver implementation
  void OnFrameDetached() override;

  bool SetUpSmoothnessReporting(
      base::ReadOnlySharedMemoryRegion& shared_memory) override;

 protected:
  // The relative and monotonic page load timings.
  struct Timing {
    Timing(mojom::PageLoadTimingPtr relative_timing,
           const PageTimingMetadataRecorder::MonotonicTiming& monotonic_timing);
    ~Timing();

    Timing(const Timing&) = delete;
    Timing& operator=(const Timing&) = delete;
    Timing(Timing&&);
    Timing& operator=(Timing&&);

    mojom::PageLoadTimingPtr relative_timing;
    PageTimingMetadataRecorder::MonotonicTiming monotonic_timing;
  };

 private:
  // Updates the metadata for the page resource associated with the given
  // request_id. Removes the request_id from the list of known ads if it is an
  // ad.
  void UpdateResourceMetadata(int request_id);

  void SendMetrics();
  void OnMetricsSenderCreated();
  virtual Timing GetTiming() const;
  virtual mojom::SoftNavigationMetricsPtr GetSoftNavigationMetrics() const;
  virtual mojom::CustomUserTimingMarkPtr GetCustomUserTimingMark() const;
  virtual std::unique_ptr<base::OneShotTimer> CreateTimer();
  virtual std::unique_ptr<PageTimingSender> CreatePageTimingSender(
      bool limited_sending_mode);
  virtual bool HasNoRenderFrame() const;
  virtual bool IsMainFrame() const;

  // Collects the data use of the frame request for a provisional load until the
  // load is committed. We want to collect data use for completed navigations in
  // this class, but the various navigation callbacks do not provide enough data
  // for us to use them for data attribution. Instead, we try to get this
  // information from ongoing resource requests on the previous page (or right
  // before this page loads in a new renderer).
  std::unique_ptr<PageResourceDataUse> provisional_frame_resource_data_use_;

  // Handle to the shared memory for transporting smoothness related ukm data.
  base::ReadOnlySharedMemoryRegion ukm_smoothness_data_;

  // The main frame intersection rectangle signal received before
  // `page_timing_metrics_sender_` is created. The signal will be send out right
  // after `page_timing_metrics_sender_` is created.
  std::optional<gfx::Rect>
      main_frame_intersection_rect_before_metrics_sender_created_;

  // Will be null when we're not actively sending metrics.
  std::unique_ptr<PageTimingMetricsSender> page_timing_metrics_sender_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_METRICS_RENDER_FRAME_OBSERVER_H_
