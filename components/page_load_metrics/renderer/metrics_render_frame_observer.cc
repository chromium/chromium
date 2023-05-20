// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/metrics_render_frame_observer.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/page_load_metrics/renderer/page_timing_metrics_sender.h"
#include "components/page_load_metrics/renderer/page_timing_sender.h"
#include "content/public/renderer/render_frame.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

// This method creates a TimeDelta from two doubles that represents timestamps
// in seconds.
base::TimeDelta CreateTimeDeltaFromTimestampsInSeconds(
    double event_time_in_seconds,
    double start_time_in_seconds) {
  if (event_time_in_seconds - start_time_in_seconds < 0) {
    event_time_in_seconds = start_time_in_seconds;
  }
  return base::Time::FromDoubleT(event_time_in_seconds) -
         base::Time::FromDoubleT(start_time_in_seconds);
}

base::TimeTicks ClampToStart(base::TimeTicks event, base::TimeTicks start) {
  return event < start ? start : event;
}

class MojoPageTimingSender : public PageTimingSender {
 public:
  explicit MojoPageTimingSender(content::RenderFrame* render_frame,
                                bool limited_sending_mode)
      : limited_sending_mode_(limited_sending_mode) {
    DCHECK(render_frame);
    render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
        &page_load_metrics_);
  }

  ~MojoPageTimingSender() override = default;

  void SendTiming(const mojom::PageLoadTimingPtr& timing,
                  const mojom::FrameMetadataPtr& metadata,
                  const std::vector<blink::UseCounterFeature>& new_features,
                  std::vector<mojom::ResourceDataUpdatePtr> resources,
                  const mojom::FrameRenderDataUpdate& render_data,
                  const mojom::CpuTimingPtr& cpu_timing,
                  mojom::InputTimingPtr input_timing_delta,
                  const absl::optional<blink::SubresourceLoadMetrics>&
                      subresource_load_metrics,
                  uint32_t soft_navigation_count) override {
    DCHECK(page_load_metrics_);
    page_load_metrics_->UpdateTiming(
        limited_sending_mode_ ? CreatePageLoadTiming() : timing->Clone(),
        metadata->Clone(), new_features, std::move(resources),
        render_data.Clone(), cpu_timing->Clone(), std::move(input_timing_delta),
        subresource_load_metrics, soft_navigation_count);
  }

  void SetUpSmoothnessReporting(
      base::ReadOnlySharedMemoryRegion shared_memory) override {
    DCHECK(page_load_metrics_);
    page_load_metrics_->SetUpSharedMemoryForSmoothness(
        std::move(shared_memory));
  }

 private:
  // Indicates that this sender should not send timing updates or frame render
  // data updates.
  // TODO(https://crbug.com/1097127): When timing updates are handled for cases
  // where we have a subframe document and no committed navigation, this can be
  // removed.
  bool limited_sending_mode_ = false;

  // Use associated interface to make sure mojo messages are ordered with regard
  // to legacy IPC messages.
  mojo::AssociatedRemote<mojom::PageLoadMetrics> page_load_metrics_;
};

}  //  namespace

MetricsRenderFrameObserver::MetricsRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      blink::WebLocalFrameObserver(render_frame ? render_frame->GetWebFrame()
                                                : nullptr) {}

MetricsRenderFrameObserver::~MetricsRenderFrameObserver() {
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->SendLatest();
}

void MetricsRenderFrameObserver::DidChangePerformanceTiming() {
  SendMetrics();
}

void MetricsRenderFrameObserver::DidObserveInputDelay(
    base::TimeDelta input_delay) {
  if (!page_timing_metrics_sender_ || HasNoRenderFrame()) {
    return;
  }
  page_timing_metrics_sender_->DidObserveInputDelay(input_delay);
}

void MetricsRenderFrameObserver::DidObserveUserInteraction(
    base::TimeDelta max_event_duration,
    blink::UserInteractionType interaction_type) {
  if (!page_timing_metrics_sender_ || HasNoRenderFrame()) {
    return;
  }
  page_timing_metrics_sender_->DidObserveUserInteraction(max_event_duration,
                                                         interaction_type);
}

void MetricsRenderFrameObserver::DidChangeCpuTiming(base::TimeDelta time) {
  if (!page_timing_metrics_sender_)
    return;
  if (HasNoRenderFrame())
    return;
  page_timing_metrics_sender_->UpdateCpuTiming(time);
}

void MetricsRenderFrameObserver::DidObserveLoadingBehavior(
    blink::LoadingBehaviorFlag behavior) {
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->DidObserveLoadingBehavior(behavior);
}

void MetricsRenderFrameObserver::DidObserveSubresourceLoad(
    const blink::SubresourceLoadMetrics& subresource_load_metrics) {
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->DidObserveSubresourceLoad(
        subresource_load_metrics);
}

void MetricsRenderFrameObserver::DidObserveNewFeatureUsage(
    const blink::UseCounterFeature& feature) {
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->DidObserveNewFeatureUsage(feature);
}

void MetricsRenderFrameObserver::DidObserveSoftNavigation(uint32_t count) {
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidObserveSoftNavigation(count);
  }
}

void MetricsRenderFrameObserver::DidObserveLayoutShift(
    double score,
    bool after_input_or_scroll) {
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->DidObserveLayoutShift(score,
                                                       after_input_or_scroll);
}

void MetricsRenderFrameObserver::DidStartResponse(
    const url::SchemeHostPort& final_response_url,
    int request_id,
    const network::mojom::URLResponseHead& response_head,
    network::mojom::RequestDestination request_destination) {
  if (provisional_frame_resource_data_use_ &&
      blink::IsRequestDestinationFrame(request_destination)) {
    // TODO(rajendrant): This frame request might start before the provisional
    // load starts, and data use of the frame request might be missed in that
    // case. There should be a guarantee that DidStartProvisionalLoad be called
    // before DidStartResponse for the frame request.
    provisional_frame_resource_data_use_->DidStartResponse(
        final_response_url, request_id, response_head, request_destination);
  } else if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidStartResponse(
        final_response_url, request_id, response_head, request_destination);
    UpdateResourceMetadata(request_id);
  }
}

void MetricsRenderFrameObserver::DidCompleteResponse(
    int request_id,
    const network::URLLoaderCompletionStatus& status) {
  if (provisional_frame_resource_data_use_ &&
      provisional_frame_resource_data_use_->resource_id() == request_id) {
    provisional_frame_resource_data_use_->DidCompleteResponse(status);
  } else if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidCompleteResponse(request_id, status);
    MaybeSetCompletedBeforeFCP(request_id);
    UpdateResourceMetadata(request_id);
  }
}

void MetricsRenderFrameObserver::DidCancelResponse(int request_id) {
  if (provisional_frame_resource_data_use_ &&
      provisional_frame_resource_data_use_->resource_id() == request_id) {
    provisional_frame_resource_data_use_.reset();
  } else if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidCancelResponse(request_id);
    UpdateResourceMetadata(request_id);
  }
}

void MetricsRenderFrameObserver::DidReceiveTransferSizeUpdate(
    int request_id,
    int received_data_length) {
  if (provisional_frame_resource_data_use_ &&
      provisional_frame_resource_data_use_->resource_id() == request_id) {
    provisional_frame_resource_data_use_->DidReceiveTransferSizeUpdate(
        received_data_length);
  } else if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidReceiveTransferSizeUpdate(
        request_id, received_data_length);
    UpdateResourceMetadata(request_id);
  }
}

void MetricsRenderFrameObserver::DidLoadResourceFromMemoryCache(
    const GURL& response_url,
    int request_id,
    int64_t encoded_body_length,
    const std::string& mime_type,
    bool from_archive) {
  // Resources from archives, such as subresources from a MHTML archive, do not
  // have valid request ids and should not be reported to PLM.
  if (from_archive)
    return;

  // A provisional frame resource cannot be serviced from the memory cache, so
  // we do not need to check |provisional_frame_resource_data_use_|.
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidLoadResourceFromMemoryCache(
        response_url, request_id, encoded_body_length, mime_type);
    MaybeSetCompletedBeforeFCP(request_id);
    UpdateResourceMetadata(request_id);
  }
}

void MetricsRenderFrameObserver::WillDetach() {
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->SendLatest();
    page_timing_metrics_sender_.reset();
  }
}

void MetricsRenderFrameObserver::DidStartNavigation(
    const GURL& url,
    absl::optional<blink::WebNavigationType> navigation_type) {
  // Send current metrics, as we might create a new RenderFrame later due to
  // this navigation (that might end up in a different process entirely, and
  // won't notify us until the current RenderFrameHost in the browser changed).
  // If that happens, it will be too late to send the metrics from WillDetach
  // or the destructor, because the browser ignores metrics update from
  // non-current RenderFrameHosts. See crbug.com/1150242 for more details.
  // TODO(crbug.com/1150242): Remove this when we have the full fix for the bug.
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->SendLatest();
}

void MetricsRenderFrameObserver::DidSetPageLifecycleState() {
  // Send current metrics, as this RenderFrame might be replaced by a new
  // RenderFrame or its process might be killed, and this might be the last
  // point we can send the metrics to the browser. See crbug.com/1150242 for
  // more details.
  // TODO(crbug.com/1150242): Remove this when we have the full fix for the bug.
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->SendLatest();
}

void MetricsRenderFrameObserver::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  // Create a new data use tracker for the new document load.
  provisional_frame_resource_data_use_ = std::make_unique<PageResourceDataUse>(
      PageResourceDataUse::kUnknownResourceId);

  // Send current metrics before the next page load commits. Don't reset here
  // as it may be a same document load.
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->SendLatest();
}

void MetricsRenderFrameObserver::DidFailProvisionalLoad() {
  // Clear the data use tracker for the provisional navigation that started.
  provisional_frame_resource_data_use_.reset();
}

void MetricsRenderFrameObserver::DidCreateDocumentElement() {
  // If we do not have a render frame or are already tracking this frame, ignore
  // the new document element.
  if (HasNoRenderFrame() || page_timing_metrics_sender_)
    return;

  // We should only track committed navigations for the main frame so ignore new
  // document elements in the main frame.
  if (render_frame()->IsMainFrame())
    return;

  // A new document element was created in a frame that did not commit a
  // provisional load. This can be due to a doc.write in the frame that aborted
  // a navigation. Create a page timing sender to track this load. This sender
  // will only send resource usage updates to the browser process. There
  // currently is not infrastructure in the browser process to monitor this case
  // and properly handle timing updates without a committed load.
  // TODO(https://crbug.com/1097127): Implement proper handling of timing
  // updates in the browser process and create a normal page timing sender.

  // It should not be possible to have a |provisional_frame_resource_data_use_|
  // object at this point. If we did, it means we reached
  // ReadyToCommitNavigation() and aborted prior to load commit which should not
  // be possible.
  DCHECK(!provisional_frame_resource_data_use_);

  Timing timing = GetTiming();
  page_timing_metrics_sender_ = std::make_unique<PageTimingMetricsSender>(
      CreatePageTimingSender(true /* limited_sending_mode */), CreateTimer(),
      std::move(timing.relative_timing), timing.monotonic_timing,
      /* initial_request=*/nullptr);

  OnMetricsSenderCreated();
}

void MetricsRenderFrameObserver::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  // Make sure to release the sender for a previous navigation, if we have one.
  page_timing_metrics_sender_.reset();

  if (HasNoRenderFrame())
    return;

  Timing timing = GetTiming();
  page_timing_metrics_sender_ = std::make_unique<PageTimingMetricsSender>(
      CreatePageTimingSender(false /* limited_sending_mode*/), CreateTimer(),
      std::move(timing.relative_timing), timing.monotonic_timing,
      std::move(provisional_frame_resource_data_use_));

  OnMetricsSenderCreated();
}

void MetricsRenderFrameObserver::SetAdResourceTracker(
    subresource_filter::AdResourceTracker* ad_resource_tracker) {
  // Remove all sources and set a new source for the observer.
  scoped_ad_resource_observation_.Reset();
  scoped_ad_resource_observation_.Observe(ad_resource_tracker);
}

void MetricsRenderFrameObserver::OnAdResourceTrackerGoingAway() {
  DCHECK(scoped_ad_resource_observation_.IsObserving());

  scoped_ad_resource_observation_.Reset();
}

void MetricsRenderFrameObserver::OnAdResourceObserved(int request_id) {
  ad_request_ids_.insert(request_id);
}

void MetricsRenderFrameObserver::OnMainFrameIntersectionChanged(
    const gfx::Rect& main_frame_intersection_rect) {
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->OnMainFrameIntersectionChanged(
        main_frame_intersection_rect);
    return;
  }

  main_frame_intersection_rect_before_metrics_sender_created_ =
      main_frame_intersection_rect;
}

void MetricsRenderFrameObserver::OnMainFrameViewportRectangleChanged(
    const gfx::Rect& main_frame_viewport_rect) {
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->OnMainFrameViewportRectangleChanged(
        main_frame_viewport_rect);
  }
}

void MetricsRenderFrameObserver::OnMainFrameImageAdRectangleChanged(
    int element_id,
    const gfx::Rect& image_ad_rect) {
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->OnMainFrameImageAdRectangleChanged(
        element_id, image_ad_rect);
  }
}

void MetricsRenderFrameObserver::OnFrameDetached() {
  WillDetach();
}

bool MetricsRenderFrameObserver::SetUpSmoothnessReporting(
    base::ReadOnlySharedMemoryRegion& shared_memory) {
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->SetUpSmoothnessReporting(
        std::move(shared_memory));
  } else {
    ukm_smoothness_data_ = std::move(shared_memory);
  }
  return true;
}

void MetricsRenderFrameObserver::MaybeSetCompletedBeforeFCP(int request_id) {
  if (HasNoRenderFrame())
    return;

  const blink::WebPerformanceMetricsForReporting& perf =
      render_frame()->GetWebFrame()->PerformanceMetricsForReporting();

  // Blink returns 0 if the performance metrics are unavailable. Check that
  // navigation start is set to determine if performance metrics are
  // available.
  if (perf.NavigationStart() == 0)
    return;

  // This should not be possible, but none the less occasionally fails in edge
  // case tests. Since we don't expect this to be valid, throw out this entry.
  // See crbug.com/1027535.
  if (base::Time::Now() < base::Time::FromDoubleT(perf.NavigationStart()))
    return;

  if (perf.FirstContentfulPaint() == 0)
    before_fcp_request_ids_.insert(request_id);
}

MetricsRenderFrameObserver::Timing::Timing(
    mojom::PageLoadTimingPtr relative_timing,
    const PageTimingMetadataRecorder::MonotonicTiming& monotonic_timing)
    : relative_timing(std::move(relative_timing)),
      monotonic_timing(monotonic_timing) {}

MetricsRenderFrameObserver::Timing::~Timing() = default;

MetricsRenderFrameObserver::Timing::Timing(Timing&&) = default;
MetricsRenderFrameObserver::Timing& MetricsRenderFrameObserver::Timing::
operator=(Timing&&) = default;

void MetricsRenderFrameObserver::UpdateResourceMetadata(int request_id) {
  DCHECK(page_timing_metrics_sender_);

  // If the request is an ad, pop it off the list of known ad requests.
  auto ad_resource_it = ad_request_ids_.find(request_id);
  bool reported_as_ad_resource =
      ad_request_ids_.find(request_id) != ad_request_ids_.end();
  if (reported_as_ad_resource)
    ad_request_ids_.erase(ad_resource_it);

  // If the request completed before fcp, pop it off the list of known
  // before-fcp requests.
  auto before_fcp_it = before_fcp_request_ids_.find(request_id);
  bool completed_before_fcp = before_fcp_it != before_fcp_request_ids_.end();
  if (completed_before_fcp) {
    before_fcp_request_ids_.erase(before_fcp_it);
  }

  bool is_main_frame_resource = render_frame()->IsMainFrame();

  if (provisional_frame_resource_data_use_ &&
      provisional_frame_resource_data_use_->resource_id() == request_id) {
    if (reported_as_ad_resource)
      provisional_frame_resource_data_use_->SetReportedAsAdResource(
          reported_as_ad_resource);
    provisional_frame_resource_data_use_->SetIsMainFrameResource(
        is_main_frame_resource);
    // Don't bother with before-fcp metrics for a provisional frame.
  } else {
    page_timing_metrics_sender_->UpdateResourceMetadata(
        request_id, reported_as_ad_resource, is_main_frame_resource,
        completed_before_fcp);
  }
}

void MetricsRenderFrameObserver::SendMetrics() {
  if (!page_timing_metrics_sender_)
    return;
  if (HasNoRenderFrame())
    return;
  Timing timing = GetTiming();
  page_timing_metrics_sender_->Update(std::move(timing.relative_timing),
                                      timing.monotonic_timing);
}

void MetricsRenderFrameObserver::OnMetricsSenderCreated() {
  if (ukm_smoothness_data_.IsValid()) {
    page_timing_metrics_sender_->SetUpSmoothnessReporting(
        std::move(ukm_smoothness_data_));
  }

  // Send the latest the frame intersection update, as otherwise we may miss
  // this information for a frame completely if there are no future updates.
  if (main_frame_intersection_rect_before_metrics_sender_created_) {
    page_timing_metrics_sender_->OnMainFrameIntersectionChanged(
        *main_frame_intersection_rect_before_metrics_sender_created_);
    main_frame_intersection_rect_before_metrics_sender_created_.reset();
  }
}

MetricsRenderFrameObserver::Timing MetricsRenderFrameObserver::GetTiming()
    const {
  const blink::WebPerformanceMetricsForReporting& perf =
      render_frame()->GetWebFrame()->PerformanceMetricsForReporting();

  mojom::PageLoadTimingPtr timing(CreatePageLoadTiming());
  PageTimingMetadataRecorder::MonotonicTiming monotonic_timing;
  double start = perf.NavigationStart();
  timing->navigation_start = base::Time::FromDoubleT(start);
  monotonic_timing.navigation_start = perf.NavigationStartAsMonotonicTime();
  if (perf.InputForNavigationStart() > 0.0) {
    timing->input_to_navigation_start = CreateTimeDeltaFromTimestampsInSeconds(
        start, perf.InputForNavigationStart());
  }
  if (perf.FirstInputDelay().has_value()) {
    timing->interactive_timing->first_input_delay = *perf.FirstInputDelay();
    monotonic_timing.first_input_delay = perf.FirstInputDelay();
  }
  if (perf.FirstInputTimestamp().has_value()) {
    timing->interactive_timing->first_input_timestamp =
        CreateTimeDeltaFromTimestampsInSeconds(
            (*perf.FirstInputTimestamp()).InSecondsF(), start);
  }
  if (perf.FirstInputTimestampAsMonotonicTime()) {
    monotonic_timing.first_input_timestamp =
        perf.FirstInputTimestampAsMonotonicTime();
  }
  if (perf.LongestInputDelay().has_value()) {
    timing->interactive_timing->longest_input_delay = *perf.LongestInputDelay();
  }
  if (perf.LongestInputTimestamp().has_value()) {
    timing->interactive_timing->longest_input_timestamp =
        CreateTimeDeltaFromTimestampsInSeconds(
            (*perf.LongestInputTimestamp()).InSecondsF(), start);
  }
  if (perf.FirstInputProcessingTime().has_value()) {
    timing->interactive_timing->first_input_processing_time =
        *perf.FirstInputProcessingTime();
  }
  if (perf.FirstScrollDelay().has_value()) {
    timing->interactive_timing->first_scroll_delay = *perf.FirstScrollDelay();
  }
  if (perf.FirstScrollTimestamp().has_value()) {
    timing->interactive_timing->first_scroll_timestamp =
        CreateTimeDeltaFromTimestampsInSeconds(
            (*perf.FirstScrollTimestamp()).InSecondsF(), start);
  }
  if (perf.ResponseStart() > 0.0)
    timing->response_start =
        CreateTimeDeltaFromTimestampsInSeconds(perf.ResponseStart(), start);
  if (perf.DomContentLoadedEventStart() > 0.0) {
    timing->document_timing->dom_content_loaded_event_start =
        CreateTimeDeltaFromTimestampsInSeconds(
            perf.DomContentLoadedEventStart(), start);
  }
  if (perf.LoadEventStart() > 0.0) {
    timing->document_timing->load_event_start =
        CreateTimeDeltaFromTimestampsInSeconds(perf.LoadEventStart(), start);
  }
  if (perf.FirstPaint() > 0.0)
    timing->paint_timing->first_paint =
        CreateTimeDeltaFromTimestampsInSeconds(perf.FirstPaint(), start);
  if (!perf.BackForwardCacheRestore().empty()) {
    blink::WebPerformanceMetricsForReporting::BackForwardCacheRestoreTimings
        restore_timings = perf.BackForwardCacheRestore();
    for (const auto& restore_timing : restore_timings) {
      double navigation_start = restore_timing.navigation_start;
      double first_paint = restore_timing.first_paint;
      absl::optional<base::TimeDelta> first_input_delay =
          restore_timing.first_input_delay;

      auto back_forward_cache_timing = mojom::BackForwardCacheTiming::New();
      if (first_paint) {
        back_forward_cache_timing
            ->first_paint_after_back_forward_cache_restore =
            CreateTimeDeltaFromTimestampsInSeconds(first_paint,
                                                   navigation_start);
      }
      for (double raf : restore_timing.request_animation_frames) {
        if (!raf)
          break;
        back_forward_cache_timing
            ->request_animation_frames_after_back_forward_cache_restore
            .push_back(
                CreateTimeDeltaFromTimestampsInSeconds(raf, navigation_start));
      }
      if (first_input_delay.has_value()) {
        back_forward_cache_timing
            ->first_input_delay_after_back_forward_cache_restore =
            first_input_delay;
      }
      timing->back_forward_cache_timings.push_back(
          std::move(back_forward_cache_timing));
    }
  }
  if (perf.FirstImagePaint() > 0.0) {
    timing->paint_timing->first_image_paint =
        CreateTimeDeltaFromTimestampsInSeconds(perf.FirstImagePaint(), start);
  }
  if (perf.FirstContentfulPaint() > 0.0) {
    DCHECK(perf.FirstEligibleToPaint() > 0);
    timing->paint_timing->first_contentful_paint =
        CreateTimeDeltaFromTimestampsInSeconds(perf.FirstContentfulPaint(),
                                               start);
    monotonic_timing.first_contentful_paint =
        ClampToStart(perf.FirstContentfulPaintAsMonotonicTime(),
                     perf.NavigationStartAsMonotonicTime());
  }
  if (perf.FirstMeaningfulPaint() > 0.0) {
    timing->paint_timing->first_meaningful_paint =
        CreateTimeDeltaFromTimestampsInSeconds(perf.FirstMeaningfulPaint(),
                                               start);
  }
  if (perf.LargestImagePaintSizeForMetrics() > 0) {
    timing->paint_timing->largest_contentful_paint->largest_image_paint_size =
        perf.LargestImagePaintSizeForMetrics();
    // Note that size can be nonzero while the time is 0 since a time of 0 is
    // sent when the image is painting. We assign the time even when it is 0 so
    // that it's not ignored, but need to be careful when doing operations on
    // the value.
    timing->paint_timing->largest_contentful_paint->largest_image_paint =
        perf.LargestImagePaintForMetrics() == 0.0
            ? base::TimeDelta()
            : CreateTimeDeltaFromTimestampsInSeconds(
                  perf.LargestImagePaintForMetrics(), start);
    timing->paint_timing->largest_contentful_paint->type =
        LargestContentfulPaintTypeToUKMFlags(
            perf.LargestContentfulPaintTypeForMetrics());
    timing->paint_timing->largest_contentful_paint->image_bpp =
        perf.LargestContentfulPaintImageBPPForMetrics();
    if (perf.LargestContentfulPaintImageRequestPriorityForMetrics()) {
      timing->paint_timing->largest_contentful_paint
          ->image_request_priority_valid = true;
      timing->paint_timing->largest_contentful_paint
          ->image_request_priority_value =
          blink::WebURLRequest::ConvertToNetPriority(
              *perf.LargestContentfulPaintImageRequestPriorityForMetrics());
    } else {
      timing->paint_timing->largest_contentful_paint
          ->image_request_priority_valid = false;
    }

    // Set largest image load type
    if (perf.LargestContentfulPaintImageLoadStart().has_value()) {
      timing->paint_timing->largest_contentful_paint->largest_image_load_start =
          CreateTimeDeltaFromTimestampsInSeconds(
              (*perf.LargestContentfulPaintImageLoadStart()).InSecondsF(),
              start);
    }
    if (perf.LargestContentfulPaintImageLoadEnd().has_value()) {
      timing->paint_timing->largest_contentful_paint->largest_image_load_end =
          CreateTimeDeltaFromTimestampsInSeconds(
              (*perf.LargestContentfulPaintImageLoadEnd()).InSecondsF(), start);
    }

    timing->paint_timing->largest_contentful_paint
        ->is_loaded_from_memory_cache =
        perf.LargestContentfulPaintImageIsLoadedFromMemoryCache();

    timing->paint_timing->largest_contentful_paint
        ->is_preloaded_with_early_hints =
        perf.LargestContentfulPaintImageIsPreloadedWithEarlyHints();
  }
  if (perf.LargestTextPaintSizeForMetrics() > 0) {
    // LargestTextPaint and LargestTextPaintSize should be available at the
    // same time. This is a renderer side DCHECK to ensure this.
    DCHECK(perf.LargestTextPaintForMetrics());
    timing->paint_timing->largest_contentful_paint->largest_text_paint =
        CreateTimeDeltaFromTimestampsInSeconds(
            perf.LargestTextPaintForMetrics(), start);
    timing->paint_timing->largest_contentful_paint->largest_text_paint_size =
        perf.LargestTextPaintSizeForMetrics();
    timing->paint_timing->largest_contentful_paint->type =
        LargestContentfulPaintTypeToUKMFlags(
            perf.LargestContentfulPaintTypeForMetrics());
  }
  // It is possible for a frame to switch from eligible for painting to
  // ineligible for it prior to the first paint. If this occurs, we need to
  // propagate the null value.
  if (perf.FirstEligibleToPaint() > 0) {
    timing->paint_timing->first_eligible_to_paint =
        CreateTimeDeltaFromTimestampsInSeconds(perf.FirstEligibleToPaint(),
                                               start);
  } else {
    timing->paint_timing->first_eligible_to_paint.reset();
  }
  if (perf.FirstInputOrScrollNotifiedTimestamp() > 0) {
    timing->paint_timing->first_input_or_scroll_notified_timestamp =
        CreateTimeDeltaFromTimestampsInSeconds(
            perf.FirstInputOrScrollNotifiedTimestamp(), start);
  }
  if (perf.ParseStart() > 0.0)
    timing->parse_timing->parse_start =
        CreateTimeDeltaFromTimestampsInSeconds(perf.ParseStart(), start);
  if (perf.ParseStop() > 0.0)
    timing->parse_timing->parse_stop =
        CreateTimeDeltaFromTimestampsInSeconds(perf.ParseStop(), start);
  if (timing->parse_timing->parse_start) {
    // If we started parsing, record all parser durations such as the amount of
    // time blocked on script load, even if those values are zero.
    timing->parse_timing->parse_blocked_on_script_load_duration =
        base::Seconds(perf.ParseBlockedOnScriptLoadDuration());
    timing->parse_timing
        ->parse_blocked_on_script_load_from_document_write_duration =
        base::Seconds(perf.ParseBlockedOnScriptLoadFromDocumentWriteDuration());
    timing->parse_timing->parse_blocked_on_script_execution_duration =
        base::Seconds(perf.ParseBlockedOnScriptExecutionDuration());
    timing->parse_timing
        ->parse_blocked_on_script_execution_from_document_write_duration =
        base::Seconds(
            perf.ParseBlockedOnScriptExecutionFromDocumentWriteDuration());
  }
  if (perf.LastPortalActivatedPaint().has_value()) {
    timing->paint_timing->portal_activated_paint =
        *perf.LastPortalActivatedPaint();
  }
  if (perf.PrerenderActivationStart().has_value())
    timing->activation_start = perf.PrerenderActivationStart();

  if (perf.UserTimingMarkFullyLoaded().has_value())
    timing->user_timing_mark_fully_loaded = perf.UserTimingMarkFullyLoaded();

  if (perf.UserTimingMarkFullyVisible().has_value())
    timing->user_timing_mark_fully_visible = perf.UserTimingMarkFullyVisible();

  if (perf.UserTimingMarkInteractive().has_value())
    timing->user_timing_mark_interactive = perf.UserTimingMarkInteractive();

  return Timing(std::move(timing), monotonic_timing);
}

std::unique_ptr<base::OneShotTimer> MetricsRenderFrameObserver::CreateTimer() {
  return std::make_unique<base::OneShotTimer>();
}

std::unique_ptr<PageTimingSender>
MetricsRenderFrameObserver::CreatePageTimingSender(bool limited_sending_mode) {
  return base::WrapUnique<PageTimingSender>(
      new MojoPageTimingSender(render_frame(), limited_sending_mode));
}

bool MetricsRenderFrameObserver::HasNoRenderFrame() const {
  bool no_frame = !render_frame() || !render_frame()->GetWebFrame();
  DCHECK(!no_frame);
  return no_frame;
}

void MetricsRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace page_load_metrics
