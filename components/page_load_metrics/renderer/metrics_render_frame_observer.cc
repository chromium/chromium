// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/metrics_render_frame_observer.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
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
  return base::Time::FromSecondsSinceUnixEpoch(event_time_in_seconds) -
         base::Time::FromSecondsSinceUnixEpoch(start_time_in_seconds);
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

  void SendTiming(
      const mojom::PageLoadTimingPtr& timing,
      const mojom::FrameMetadataPtr& metadata,
      const std::vector<blink::UseCounterFeature>& new_features,
      std::vector<mojom::ResourceDataUpdatePtr> resources,
      const mojom::FrameRenderDataUpdate& render_data,
      const mojom::CpuTimingPtr& cpu_timing,
      mojom::InputTimingPtr input_timing_delta,
      const std::optional<blink::SubresourceLoadMetrics>&
          subresource_load_metrics,
      const mojom::SoftNavigationMetricsPtr& soft_navigation_metrics) override {
    DCHECK(page_load_metrics_);
    page_load_metrics_->UpdateTiming(
        limited_sending_mode_ ? CreatePageLoadTiming() : timing->Clone(),
        metadata->Clone(), new_features, std::move(resources),
        render_data.Clone(), cpu_timing->Clone(), std::move(input_timing_delta),
        subresource_load_metrics, soft_navigation_metrics->Clone());
  }

  void SetUpSmoothnessReporting(
      base::ReadOnlySharedMemoryRegion shared_memory) override {
    DCHECK(page_load_metrics_);
    page_load_metrics_->SetUpSharedMemoryForSmoothness(
        std::move(shared_memory));
  }

  void SendCustomUserTiming(mojom::CustomUserTimingMarkPtr timing) override {
    CHECK(timing);
    CHECK(page_load_metrics_);
    page_load_metrics_->AddCustomUserTiming(std::move(timing));
  }

 private:
  // Indicates that this sender should not send timing updates or frame render
  // data updates.
  // TODO(crbug.com/40136524): When timing updates are handled for cases
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
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->SendLatest();
  }
}

void MetricsRenderFrameObserver::DidChangePerformanceTiming() {
  SendMetrics();
}

void MetricsRenderFrameObserver::DidObserveUserInteraction(
    base::TimeTicks max_event_start,
    base::TimeTicks max_event_queued_main_thread,
    base::TimeTicks max_event_commit_finish,
    base::TimeTicks max_event_end,
    blink::UserInteractionType interaction_type,
    uint64_t interaction_offset) {
  if (!page_timing_metrics_sender_ || HasNoRenderFrame()) {
    return;
  }
  page_timing_metrics_sender_->DidObserveUserInteraction(
      max_event_start, max_event_queued_main_thread, max_event_commit_finish,
      max_event_end, interaction_type, interaction_offset);
}

void MetricsRenderFrameObserver::DidChangeCpuTiming(base::TimeDelta time) {
  if (!page_timing_metrics_sender_) {
    return;
  }
  if (HasNoRenderFrame()) {
    return;
  }
  page_timing_metrics_sender_->UpdateCpuTiming(time);
}

void MetricsRenderFrameObserver::DidObserveLoadingBehavior(
    blink::LoadingBehaviorFlag behavior) {
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidObserveLoadingBehavior(behavior);
  }
}

void MetricsRenderFrameObserver::DidObserveJavaScriptFrameworks(
    const blink::JavaScriptFrameworkDetectionResult& result) {
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidObserveJavaScriptFrameworks(result);
  }
}

void MetricsRenderFrameObserver::DidObserveSubresourceLoad(
    const blink::SubresourceLoadMetrics& subresource_load_metrics) {
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidObserveSubresourceLoad(
        subresource_load_metrics);
  }
}

void MetricsRenderFrameObserver::DidObserveNewFeatureUsage(
    const blink::UseCounterFeature& feature) {
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidObserveNewFeatureUsage(feature);
  }
}

void MetricsRenderFrameObserver::DidObserveSoftNavigation(
    blink::SoftNavigationMetrics soft_nav_metrics) {
  if (page_timing_metrics_sender_) {
    const blink::WebPerformanceMetricsForReporting& metrics =
        render_frame()->GetWebFrame()->PerformanceMetricsForReporting();

    // Make soft navigation start time relative to navigation start.
    soft_nav_metrics.start_time = CreateTimeDeltaFromTimestampsInSeconds(
        soft_nav_metrics.start_time.InSecondsF(), metrics.NavigationStart());

    // (crbug.com/40074158): will non-fatally dump in official builds if the
    // start_time is 0.
    DUMP_WILL_BE_CHECK(!soft_nav_metrics.start_time.is_zero());

    page_timing_metrics_sender_->DidObserveSoftNavigation(soft_nav_metrics);
  }
}

void MetricsRenderFrameObserver::DidObserveLayoutShift(
    double score,
    bool after_input_or_scroll) {
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidObserveLayoutShift(score,
                                                       after_input_or_scroll);
  }
}

void MetricsRenderFrameObserver::DidStartResponse(
    const url::SchemeHostPort& final_response_url,
    int request_id,
    const network::mojom::URLResponseHead& response_head,
    network::mojom::RequestDestination request_destination,
    bool is_ad_resource) {
  if (provisional_frame_resource_data_use_ &&
      blink::IsRequestDestinationFrame(request_destination)) {
    // TODO(rajendrant): This frame request might start before the provisional
    // load starts, and data use of the frame request might be missed in that
    // case. There should be a guarantee that DidStartProvisionalLoad be called
    // before DidStartResponse for the frame request.
    provisional_frame_resource_data_use_->DidStartResponse(
        final_response_url, request_id, response_head, request_destination,
        is_ad_resource);
  } else if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidStartResponse(
        final_response_url, request_id, response_head, request_destination,
        is_ad_resource);
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
  if (from_archive) {
    return;
  }

  // A provisional frame resource cannot be serviced from the memory cache, so
  // we do not need to check |provisional_frame_resource_data_use_|.
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidLoadResourceFromMemoryCache(
        response_url, request_id, encoded_body_length, mime_type);
    UpdateResourceMetadata(request_id);
  }
}

void MetricsRenderFrameObserver::WillDetach(blink::DetachReason detach_reason) {
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->SendLatest();
    page_timing_metrics_sender_.reset();
  }
}

void MetricsRenderFrameObserver::DidStartNavigation(
    const GURL& url,
    std::optional<blink::WebNavigationType> navigation_type) {
  // Send current metrics, as we might create a new RenderFrame later due to
  // this navigation (that might end up in a different process entirely, and
  // won't notify us until the current RenderFrameHost in the browser changed).
  // If that happens, it will be too late to send the metrics from WillDetach
  // or the destructor, because the browser ignores metrics update from
  // non-current RenderFrameHosts. See crbug.com/1150242 for more details.
  // TODO(crbug.com/40157795): Remove this when we have the full fix for the
  // bug.
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->SendLatest();
  }
}

void MetricsRenderFrameObserver::DidSetPageLifecycleState(
    bool restoring_from_bfcache) {
  // Send current metrics, as this RenderFrame might be replaced by a new
  // RenderFrame or its process might be killed, and this might be the last
  // point we can send the metrics to the browser. See crbug.com/1150242 for
  // more details.
  // TODO(crbug.com/40157795): Remove this when we have the full fix for the
  // bug.
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->SendLatest();
  }
}

void MetricsRenderFrameObserver::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  // Create a new data use tracker for the new document load.
  provisional_frame_resource_data_use_ = std::make_unique<PageResourceDataUse>(
      PageResourceDataUse::kUnknownResourceId);

  // Send current metrics before the next page load commits. Don't reset here
  // as it may be a same document load.
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->SendLatest();
  }
}

void MetricsRenderFrameObserver::DidFailProvisionalLoad() {
  // Clear the data use tracker for the provisional navigation that started.
  provisional_frame_resource_data_use_.reset();
}

void MetricsRenderFrameObserver::DidCreateDocumentElement() {
  // If we do not have a render frame or are already tracking this frame, ignore
  // the new document element.
  if (HasNoRenderFrame() || page_timing_metrics_sender_) {
    return;
  }

  // We should only track committed navigations for the main frame so ignore new
  // document elements in the main frame.
  if (IsMainFrame()) {
    return;
  }

  // A new document element was created in a frame that did not commit a
  // provisional load. This can be due to a doc.write in the frame that aborted
  // a navigation. Create a page timing sender to track this load. This sender
  // will only send resource usage updates to the browser process. There
  // currently is not infrastructure in the browser process to monitor this case
  // and properly handle timing updates without a committed load.
  // TODO(crbug.com/40136524): Implement proper handling of timing
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
      /* initial_request=*/nullptr, /* is_main_frame=*/false);

  OnMetricsSenderCreated();
}

void MetricsRenderFrameObserver::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  // Make sure to release the sender for a previous navigation, if we have one.
  page_timing_metrics_sender_.reset();

  if (HasNoRenderFrame()) {
    return;
  }

  Timing timing = GetTiming();
  page_timing_metrics_sender_ = std::make_unique<PageTimingMetricsSender>(
      CreatePageTimingSender(false /* limited_sending_mode*/), CreateTimer(),
      std::move(timing.relative_timing), timing.monotonic_timing,
      std::move(provisional_frame_resource_data_use_), IsMainFrame());

  OnMetricsSenderCreated();
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
  WillDetach(blink::DetachReason::kNavigation);
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

MetricsRenderFrameObserver::Timing::Timing(
    mojom::PageLoadTimingPtr relative_timing,
    const PageTimingMetadataRecorder::MonotonicTiming& monotonic_timing)
    : relative_timing(std::move(relative_timing)),
      monotonic_timing(monotonic_timing) {}

MetricsRenderFrameObserver::Timing::~Timing() = default;

MetricsRenderFrameObserver::Timing::Timing(Timing&&) = default;
MetricsRenderFrameObserver::Timing&
MetricsRenderFrameObserver::Timing::operator=(Timing&&) = default;

void MetricsRenderFrameObserver::UpdateResourceMetadata(int request_id) {
  DCHECK(page_timing_metrics_sender_);

  bool is_main_frame_resource = IsMainFrame();

  if (provisional_frame_resource_data_use_ &&
      provisional_frame_resource_data_use_->resource_id() == request_id) {
    provisional_frame_resource_data_use_->SetIsMainFrameResource(
        is_main_frame_resource);
    // Don't bother with before-fcp metrics for a provisional frame.
  } else {
    page_timing_metrics_sender_->UpdateResourceMetadata(request_id,
                                                        is_main_frame_resource);
  }
}

void MetricsRenderFrameObserver::SendMetrics() {
  if (!page_timing_metrics_sender_) {
    return;
  }
  if (HasNoRenderFrame()) {
    return;
  }
  Timing timing = GetTiming();
  page_timing_metrics_sender_->UpdateSoftNavigationMetrics(
      GetSoftNavigationMetrics());
  page_timing_metrics_sender_->Update(std::move(timing.relative_timing),
                                      timing.monotonic_timing);

  mojom::CustomUserTimingMarkPtr user_timing = GetCustomUserTimingMark();
  if (user_timing) {
    page_timing_metrics_sender_->SendCustomUserTimingMark(
        std::move(user_timing));
  }
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

mojom::SoftNavigationMetricsPtr
MetricsRenderFrameObserver::GetSoftNavigationMetrics() const {
  CHECK(render_frame());
  CHECK(render_frame()->GetWebFrame());
  const blink::WebPerformanceMetricsForReporting& metrics =
      render_frame()->GetWebFrame()->PerformanceMetricsForReporting();
  CHECK(page_timing_metrics_sender_.get());
  auto soft_navigation_metrics =
      page_timing_metrics_sender_->GetSoftNavigationMetrics();

  CHECK(!soft_navigation_metrics.is_null());

  soft_navigation_metrics->largest_contentful_paint =
      CreateLargestContentfulPaintTiming();

  auto soft_navigation_lcp_details_ =
      metrics.SoftNavigationLargestContentfulDetailsForMetrics();

  double soft_navigation_start_relative_to_navigation_start =
      soft_navigation_metrics->start_time.InSecondsF();

  double navigation_start = metrics.NavigationStart();

  if (soft_navigation_lcp_details_.image_paint_size > 0) {
    // Set largest image time.
    // Note that size can be nonzero while the time is 0 since a time of 0 is
    // sent when the image is painting. We assign the time even when it is 0 so
    // that it's not ignored, but need to be careful when doing operations on
    // the value.
    if (soft_navigation_lcp_details_.image_paint_time == 0.0) {
      soft_navigation_metrics->largest_contentful_paint->largest_image_paint =
          base::TimeDelta();
    } else {
      base::TimeDelta image_paint_time_relative_to_navigation_start =
          CreateTimeDeltaFromTimestampsInSeconds(
              soft_navigation_lcp_details_.image_paint_time, navigation_start);

      base::TimeDelta image_paint_time_relative_to_soft_navigation_start =
          CreateTimeDeltaFromTimestampsInSeconds(
              image_paint_time_relative_to_navigation_start.InSecondsF(),
              soft_navigation_start_relative_to_navigation_start);

      soft_navigation_metrics->largest_contentful_paint->largest_image_paint =
          image_paint_time_relative_to_soft_navigation_start;
    }
    // Set largest image size.
    soft_navigation_metrics->largest_contentful_paint
        ->largest_image_paint_size =
        soft_navigation_lcp_details_.image_paint_size;

    // Set largest image load type.
    soft_navigation_metrics->largest_contentful_paint->type =
        LargestContentfulPaintTypeToUKMFlags(soft_navigation_lcp_details_.type);

    // Set largest image bpp value.
    soft_navigation_metrics->largest_contentful_paint->image_bpp =
        soft_navigation_lcp_details_.image_bpp;

    // Set largest image request priority.
    if (soft_navigation_lcp_details_.image_request_priority.has_value()) {
      soft_navigation_metrics->largest_contentful_paint
          ->image_request_priority_valid = true;
      soft_navigation_metrics->largest_contentful_paint
          ->image_request_priority_value =
          blink::WebURLRequest::ConvertToNetPriority(
              soft_navigation_lcp_details_.image_request_priority.value());
    } else {
      soft_navigation_metrics->largest_contentful_paint
          ->image_request_priority_valid = false;
    }

    // Set largest image discovery time.
    if (soft_navigation_lcp_details_.resource_load_timings.discovery_time
            .has_value()) {
      base::TimeDelta image_discovery_time_relative_to_navigation_start =
          CreateTimeDeltaFromTimestampsInSeconds(
              (soft_navigation_lcp_details_.resource_load_timings.discovery_time
                   .value())
                  .InSecondsF(),
              navigation_start);

      base::TimeDelta image_discovery_time_relative_to_soft_navigation_start =
          CreateTimeDeltaFromTimestampsInSeconds(
              image_discovery_time_relative_to_navigation_start.InSecondsF(),
              soft_navigation_start_relative_to_navigation_start);

      soft_navigation_metrics->largest_contentful_paint->resource_load_timings
          ->discovery_time =
          image_discovery_time_relative_to_soft_navigation_start;
    }

    // Set largest image load start.
    if (soft_navigation_lcp_details_.resource_load_timings.load_start
            .has_value()) {
      base::TimeDelta image_load_start_relative_to_navigation_start =
          CreateTimeDeltaFromTimestampsInSeconds(
              (soft_navigation_lcp_details_.resource_load_timings.load_start
                   .value())
                  .InSecondsF(),
              navigation_start);

      base::TimeDelta image_load_start_relative_to_soft_navigation_start =
          CreateTimeDeltaFromTimestampsInSeconds(
              image_load_start_relative_to_navigation_start.InSecondsF(),
              soft_navigation_start_relative_to_navigation_start);

      soft_navigation_metrics->largest_contentful_paint->resource_load_timings
          ->load_start = image_load_start_relative_to_soft_navigation_start;
    }

    // Set largest image load end.
    if (soft_navigation_lcp_details_.resource_load_timings.load_end
            .has_value()) {
      base::TimeDelta image_load_end_relative_to_navigation_start =
          CreateTimeDeltaFromTimestampsInSeconds(
              (soft_navigation_lcp_details_.resource_load_timings.load_end
                   .value())
                  .InSecondsF(),
              navigation_start);

      base::TimeDelta image_load_end_relative_to_soft_navigation_start =
          CreateTimeDeltaFromTimestampsInSeconds(
              image_load_end_relative_to_navigation_start.InSecondsF(),
              soft_navigation_start_relative_to_navigation_start);

      soft_navigation_metrics->largest_contentful_paint->resource_load_timings
          ->load_end = image_load_end_relative_to_soft_navigation_start;
    }
  }

  if (soft_navigation_lcp_details_.text_paint_size > 0) {
    // LargestTextPaint and LargestTextPaintSize should be available at the
    // same time. This is a renderer side DCHECK to ensure this.
    DCHECK(soft_navigation_lcp_details_.text_paint_time);

    base::TimeDelta text_paint_time_relative_to_navigation_start =
        CreateTimeDeltaFromTimestampsInSeconds(
            soft_navigation_lcp_details_.text_paint_time, navigation_start);

    base::TimeDelta text_paint_time_relative_to_soft_navigation_start =
        CreateTimeDeltaFromTimestampsInSeconds(
            text_paint_time_relative_to_navigation_start.InSecondsF(),
            soft_navigation_start_relative_to_navigation_start);

    soft_navigation_metrics->largest_contentful_paint->largest_text_paint =
        text_paint_time_relative_to_soft_navigation_start;

    soft_navigation_metrics->largest_contentful_paint->largest_text_paint_size =
        soft_navigation_lcp_details_.text_paint_size;

    soft_navigation_metrics->largest_contentful_paint->type =
        LargestContentfulPaintTypeToUKMFlags(soft_navigation_lcp_details_.type);
  }

  return soft_navigation_metrics;
}

MetricsRenderFrameObserver::Timing MetricsRenderFrameObserver::GetTiming()
    const {
  const blink::WebPerformanceMetricsForReporting& perf =
      render_frame()->GetWebFrame()->PerformanceMetricsForReporting();

  mojom::PageLoadTimingPtr timing(CreatePageLoadTiming());
  PageTimingMetadataRecorder::MonotonicTiming monotonic_timing;
  double start = perf.NavigationStart();
  timing->navigation_start = base::Time::FromSecondsSinceUnixEpoch(start);
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
  if (perf.FirstScrollDelay().has_value()) {
    timing->interactive_timing->first_scroll_delay = *perf.FirstScrollDelay();
  }
  if (perf.FirstScrollTimestamp().has_value()) {
    timing->interactive_timing->first_scroll_timestamp =
        CreateTimeDeltaFromTimestampsInSeconds(
            (*perf.FirstScrollTimestamp()).InSecondsF(), start);
  }
  if (perf.DomainLookupStart() > 0.0) {
    timing->domain_lookup_timing->domain_lookup_start =
        CreateTimeDeltaFromTimestampsInSeconds(perf.DomainLookupStart(), start);
  }
  if (perf.DomainLookupEnd() > 0.0) {
    timing->domain_lookup_timing->domain_lookup_end =
        CreateTimeDeltaFromTimestampsInSeconds(perf.DomainLookupEnd(), start);
  }
  if (perf.ConnectStart() > 0.0) {
    timing->connect_start =
        CreateTimeDeltaFromTimestampsInSeconds(perf.ConnectStart(), start);
  }
  if (perf.ResponseStart() > 0.0) {
    timing->response_start =
        CreateTimeDeltaFromTimestampsInSeconds(perf.ResponseStart(), start);
  }
  if (perf.DomContentLoadedEventStart() > 0.0) {
    timing->document_timing->dom_content_loaded_event_start =
        CreateTimeDeltaFromTimestampsInSeconds(
            perf.DomContentLoadedEventStart(), start);
  }
  if (perf.LoadEventStart() > 0.0) {
    timing->document_timing->load_event_start =
        CreateTimeDeltaFromTimestampsInSeconds(perf.LoadEventStart(), start);
  }
  if (perf.FirstPaint() > 0.0) {
    timing->paint_timing->first_paint =
        CreateTimeDeltaFromTimestampsInSeconds(perf.FirstPaint(), start);
  }
  if (!perf.BackForwardCacheRestore().empty()) {
    blink::WebPerformanceMetricsForReporting::BackForwardCacheRestoreTimings
        restore_timings = perf.BackForwardCacheRestore();
    for (const auto& restore_timing : restore_timings) {
      double navigation_start = restore_timing.navigation_start;
      double first_paint = restore_timing.first_paint;
      std::optional<base::TimeDelta> first_input_delay =
          restore_timing.first_input_delay;

      auto back_forward_cache_timing = mojom::BackForwardCacheTiming::New();
      if (first_paint) {
        back_forward_cache_timing
            ->first_paint_after_back_forward_cache_restore =
            CreateTimeDeltaFromTimestampsInSeconds(first_paint,
                                                   navigation_start);
      }
      for (double raf : restore_timing.request_animation_frames) {
        if (!raf) {
          break;
        }
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
  blink::LargestContentfulPaintDetailsForReporting
      largest_contentful_paint_details =
          perf.LargestContentfulDetailsForMetrics();
  monotonic_timing.frame_largest_contentful_paint =
      largest_contentful_paint_details.merged_unclamped_paint_time;

  if (largest_contentful_paint_details.image_paint_size > 0) {
    timing->paint_timing->largest_contentful_paint->largest_image_paint_size =
        largest_contentful_paint_details.image_paint_size;
    // Note that size can be nonzero while the time is 0 since a time of 0 is
    // sent when the image is painting. We assign the time even when it is 0 so
    // that it's not ignored, but need to be careful when doing operations on
    // the value.
    timing->paint_timing->largest_contentful_paint->largest_image_paint =
        largest_contentful_paint_details.image_paint_time == 0.0
            ? base::TimeDelta()
            : CreateTimeDeltaFromTimestampsInSeconds(
                  largest_contentful_paint_details.image_paint_time, start);

    timing->paint_timing->largest_contentful_paint->type =
        LargestContentfulPaintTypeToUKMFlags(
            largest_contentful_paint_details.type);

    timing->paint_timing->largest_contentful_paint->image_bpp =
        largest_contentful_paint_details.image_bpp;

    if (largest_contentful_paint_details.image_request_priority.has_value()) {
      timing->paint_timing->largest_contentful_paint
          ->image_request_priority_valid = true;
      timing->paint_timing->largest_contentful_paint
          ->image_request_priority_value =
          blink::WebURLRequest::ConvertToNetPriority(
              largest_contentful_paint_details.image_request_priority.value());
    } else {
      timing->paint_timing->largest_contentful_paint
          ->image_request_priority_valid = false;
    }

    // Set largest image load timings.
    if (largest_contentful_paint_details.resource_load_timings.discovery_time
            .has_value()) {
      timing->paint_timing->largest_contentful_paint->resource_load_timings
          ->discovery_time = CreateTimeDeltaFromTimestampsInSeconds(
          (largest_contentful_paint_details.resource_load_timings.discovery_time
               .value())
              .InSecondsF(),
          start);
    }

    if (largest_contentful_paint_details.resource_load_timings.load_start
            .has_value()) {
      timing->paint_timing->largest_contentful_paint->resource_load_timings
          ->load_start = CreateTimeDeltaFromTimestampsInSeconds(
          (largest_contentful_paint_details.resource_load_timings.load_start
               .value())
              .InSecondsF(),
          start);
    }

    if (largest_contentful_paint_details.resource_load_timings.load_end
            .has_value()) {
      timing->paint_timing->largest_contentful_paint->resource_load_timings
          ->load_end = CreateTimeDeltaFromTimestampsInSeconds(
          (largest_contentful_paint_details.resource_load_timings.load_end
               .value())
              .InSecondsF(),
          start);
    }
  }
  if (largest_contentful_paint_details.text_paint_size > 0) {
    // LargestTextPaint and LargestTextPaintSize should be available at the
    // same time. This is a renderer side DCHECK to ensure this.
    DCHECK(largest_contentful_paint_details.text_paint_time);

    timing->paint_timing->largest_contentful_paint->largest_text_paint =
        CreateTimeDeltaFromTimestampsInSeconds(
            largest_contentful_paint_details.text_paint_time, start);

    timing->paint_timing->largest_contentful_paint->largest_text_paint_size =
        largest_contentful_paint_details.text_paint_size;

    timing->paint_timing->largest_contentful_paint->type =
        LargestContentfulPaintTypeToUKMFlags(
            largest_contentful_paint_details.type);
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
  if (perf.ParseStart() > 0.0) {
    timing->parse_timing->parse_start =
        CreateTimeDeltaFromTimestampsInSeconds(perf.ParseStart(), start);
  }
  if (perf.ParseStop() > 0.0) {
    timing->parse_timing->parse_stop =
        CreateTimeDeltaFromTimestampsInSeconds(perf.ParseStop(), start);
  }
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
  if (perf.PrerenderActivationStart().has_value()) {
    timing->activation_start = perf.PrerenderActivationStart();
  }

  if (perf.UserTimingMarkFullyLoaded().has_value()) {
    timing->user_timing_mark_fully_loaded = perf.UserTimingMarkFullyLoaded();
  }

  if (perf.UserTimingMarkFullyVisible().has_value()) {
    timing->user_timing_mark_fully_visible = perf.UserTimingMarkFullyVisible();
  }

  if (perf.UserTimingMarkInteractive().has_value()) {
    timing->user_timing_mark_interactive = perf.UserTimingMarkInteractive();
  }

  return Timing(std::move(timing), monotonic_timing);
}

mojom::CustomUserTimingMarkPtr
MetricsRenderFrameObserver::GetCustomUserTimingMark() const {
  const blink::WebPerformanceMetricsForReporting& perf =
      render_frame()->GetWebFrame()->PerformanceMetricsForReporting();
  auto timing = perf.CustomUserTimingMark();
  if (!timing.has_value()) {
    return nullptr;
  }
  const auto [mark_name, start_time] = timing.value();
  mojom::CustomUserTimingMarkPtr custom_user_timing_mark =
      mojom::CustomUserTimingMark::New();
  custom_user_timing_mark->mark_name = mark_name;
  custom_user_timing_mark->start_time = start_time;

  return custom_user_timing_mark;
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

bool MetricsRenderFrameObserver::IsMainFrame() const {
  return render_frame()->IsMainFrame();
}

void MetricsRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace page_load_metrics
