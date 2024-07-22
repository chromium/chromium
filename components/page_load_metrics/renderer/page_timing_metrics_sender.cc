// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_timing_metrics_sender.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/renderer/page_timing_sender.h"
#include "components/page_load_metrics/renderer/soft_navigation_metrics_type_converter.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/javascript_framework_detection.mojom-forward.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"

namespace page_load_metrics {

namespace {
const int kInitialTimerDelayMillis = 50;

mojom::UserInteractionType UserInteractionTypeForMojom(
    blink::UserInteractionType interaction_type) {
  switch (interaction_type) {
    case blink::UserInteractionType::kKeyboard:
      return mojom::UserInteractionType::kKeyboard;
    case blink::UserInteractionType::kTapOrClick:
      return mojom::UserInteractionType::kTapOrClick;
    case blink::UserInteractionType::kDrag:
      return mojom::UserInteractionType::kDrag;
  }
  // mojom::UserInteractionType should have the same interaction types as
  // blink::UserInteractionType does.
  NOTREACHED_IN_MIGRATION();
  return mojom::UserInteractionType::kMinValue;
}

bool IsFirstFCP(const mojom::PageLoadTimingPtr& last_timing,
                const mojom::PageLoadTimingPtr& new_timing) {
  return (!last_timing->paint_timing ||
          !last_timing->paint_timing->first_contentful_paint.has_value()) &&
         new_timing->paint_timing &&
         new_timing->paint_timing->first_contentful_paint.has_value();
}

bool IsFirstParseStart(const mojom::PageLoadTimingPtr& last_timing,
                       const mojom::PageLoadTimingPtr& new_timing) {
  return (!last_timing->parse_timing ||
          !last_timing->parse_timing->parse_start.has_value()) &&
         new_timing->parse_timing &&
         new_timing->parse_timing->parse_start.has_value();
}

bool IsFirstDCL(const mojom::PageLoadTimingPtr& last_timing,
                const mojom::PageLoadTimingPtr& new_timing) {
  return (!last_timing->document_timing ||
          !last_timing->document_timing->dom_content_loaded_event_start
               .has_value()) &&
         new_timing->document_timing &&
         new_timing->document_timing->dom_content_loaded_event_start
             .has_value();
}

}  // namespace

PageTimingMetricsSender::PageTimingMetricsSender(
    std::unique_ptr<PageTimingSender> sender,
    std::unique_ptr<base::OneShotTimer> timer,
    mojom::PageLoadTimingPtr initial_timing,
    const PageTimingMetadataRecorder::MonotonicTiming& initial_monotonic_timing,
    std::unique_ptr<PageResourceDataUse> initial_request,
    bool is_main_frame)
    : sender_(std::move(sender)),
      timer_(std::move(timer)),
      last_timing_(std::move(initial_timing)),
      last_cpu_timing_(mojom::CpuTiming::New()),
      input_timing_delta_(mojom::InputTiming::New()),
      metadata_(mojom::FrameMetadata::New()),
      soft_navigation_metrics_(CreateSoftNavigationMetrics()),
      buffer_timer_delay_ms_(GetBufferTimerDelayMillis(TimerType::kRenderer)),
      metadata_recorder_(initial_monotonic_timing, is_main_frame) {
  InitiateUserInteractionTiming();
  if (initial_request) {
    int resource_id = initial_request->resource_id();
    page_resource_data_use_[resource_id] = std::move(initial_request);
  }
  if (!IsEmpty(*last_timing_)) {
    EnsureSendTimer();
  }
}

PageTimingMetricsSender::~PageTimingMetricsSender() {
  // Make sure we don't have any unsent data. If this assertion fails, then it
  // means metrics are somehow coming in between MetricsRenderFrameObserver's
  // ReadyToCommitNavigation and DidCommitProvisionalLoad.
  DCHECK(!timer_->IsRunning());
}

void PageTimingMetricsSender::DidObserveLoadingBehavior(
    blink::LoadingBehaviorFlag behavior) {
  if (behavior & metadata_->behavior_flags) {
    return;
  }
  metadata_->behavior_flags |= behavior;
  EnsureSendTimer();
}

void PageTimingMetricsSender::DidObserveJavaScriptFrameworks(
    const blink::JavaScriptFrameworkDetectionResult& result) {
  metadata_->framework_detection_result = result;
  EnsureSendTimer();
}

void PageTimingMetricsSender::DidObserveSubresourceLoad(
    const blink::SubresourceLoadMetrics& subresource_load_metrics) {
  if (subresource_load_metrics_ &&
      *subresource_load_metrics_ == subresource_load_metrics) {
    return;
  }
  subresource_load_metrics_ = subresource_load_metrics;
  EnsureSendTimer();
}

void PageTimingMetricsSender::DidObserveNewFeatureUsage(
    const blink::UseCounterFeature& feature) {
  if (feature_tracker_.TestAndSet(feature))
    return;

  new_features_.push_back(feature);
  EnsureSendTimer();
}

void PageTimingMetricsSender::DidObserveSoftNavigation(
    blink::SoftNavigationMetrics new_metrics) {
  // The start_time is a TimeDelta, and its resolution is in microseconds.
  // Each soft navigations would have an effectively larger start time than the
  // previous one. Each soft navigation should also have a larger count and a
  // different navigation id than the previous one.
  CHECK(new_metrics.count > soft_navigation_metrics_->count);
  CHECK(!new_metrics.start_time.is_zero());
  CHECK(new_metrics.start_time > soft_navigation_metrics_->start_time);
  CHECK(new_metrics.navigation_id != soft_navigation_metrics_->navigation_id);

  soft_navigation_metrics_->count = new_metrics.count;

  soft_navigation_metrics_->start_time = new_metrics.start_time;

  soft_navigation_metrics_->navigation_id = new_metrics.navigation_id;

  EnsureSendTimer();
}

void PageTimingMetricsSender::DidObserveLayoutShift(
    double score,
    bool after_input_or_scroll) {
  DCHECK(score > 0);
  render_data_.layout_shift_delta += score;
  render_data_.new_layout_shifts.push_back(
      mojom::LayoutShift::New(base::TimeTicks::Now(), score));
  if (!after_input_or_scroll)
    render_data_.layout_shift_delta_before_input_or_scroll += score;
  EnsureSendTimer();
}

void PageTimingMetricsSender::DidStartResponse(
    const url::SchemeHostPort& final_response_url,
    int resource_id,
    const network::mojom::URLResponseHead& response_head,
    network::mojom::RequestDestination request_destination,
    bool is_ad_resource) {
  // There can be multiple `DidStartResponse` for the same resource id
  // (crbug.com/1504430).
  FindOrInsertPageResourceDataUse(resource_id)
      ->DidStartResponse(final_response_url, resource_id, response_head,
                         request_destination, is_ad_resource);
}

void PageTimingMetricsSender::DidReceiveTransferSizeUpdate(
    int resource_id,
    int received_data_length) {
  // Transfer size updates are called in a throttled manner.
  auto resource_it = page_resource_data_use_.find(resource_id);

  // It is possible that resources are not in the map, if response headers were
  // not received or for failed/cancelled resources.
  if (resource_it == page_resource_data_use_.end()) {
    return;
  }

  resource_it->second->DidReceiveTransferSizeUpdate(received_data_length);
  modified_resources_.insert(resource_it->second.get());
  EnsureSendTimer();
}

void PageTimingMetricsSender::DidCompleteResponse(
    int resource_id,
    const network::URLLoaderCompletionStatus& status) {
  PageResourceDataUse* data_use = FindOrInsertPageResourceDataUse(resource_id);
  data_use->DidCompleteResponse(status);
  modified_resources_.insert(data_use);
  EnsureSendTimer();
}

void PageTimingMetricsSender::DidCancelResponse(int resource_id) {
  auto resource_it = page_resource_data_use_.find(resource_id);
  if (resource_it == page_resource_data_use_.end()) {
    return;
  }
  resource_it->second->DidCancelResponse();
}

void PageTimingMetricsSender::DidLoadResourceFromMemoryCache(
    const GURL& response_url,
    int request_id,
    int64_t encoded_body_length,
    const std::string& mime_type) {
  // In general, we should not observe the same resource being loaded twice in
  // the frame. This is possible due to an existing workaround in
  // ResourceFetcher::EmulateLoadStartedForInspector(). In this case, ignore
  // multiple resources being loaded in the document, as memory cache resources
  // are only reported once per context by design in all other cases.
  if (base::Contains(page_resource_data_use_, request_id))
    return;

  FindOrInsertPageResourceDataUse(request_id)
      ->DidLoadFromMemoryCache(response_url, encoded_body_length, mime_type);
}

void PageTimingMetricsSender::OnMainFrameIntersectionChanged(
    const gfx::Rect& main_frame_intersection_rect) {
  metadata_->main_frame_intersection_rect = main_frame_intersection_rect;
  EnsureSendTimer();
}

void PageTimingMetricsSender::OnMainFrameViewportRectangleChanged(
    const gfx::Rect& main_frame_viewport_rect) {
  metadata_->main_frame_viewport_rect = main_frame_viewport_rect;
  EnsureSendTimer();
}

void PageTimingMetricsSender::OnMainFrameImageAdRectangleChanged(
    int element_id,
    const gfx::Rect& image_ad_rect) {
  metadata_->main_frame_image_ad_rects[element_id] = image_ad_rect;
  EnsureSendTimer();
}

void PageTimingMetricsSender::UpdateResourceMetadata(
    int resource_id,
    bool is_main_frame_resource) {
  auto it = page_resource_data_use_.find(resource_id);
  if (it == page_resource_data_use_.end())
    return;

  it->second->SetIsMainFrameResource(is_main_frame_resource);
}

void PageTimingMetricsSender::SetUpSmoothnessReporting(
    base::ReadOnlySharedMemoryRegion shared_memory) {
  sender_->SetUpSmoothnessReporting(std::move(shared_memory));
}

void PageTimingMetricsSender::Update(
    mojom::PageLoadTimingPtr timing,
    const PageTimingMetadataRecorder::MonotonicTiming& monotonic_timing) {
  if (last_timing_->Equals(*timing)) {
    return;
  }

  // We want to make sure that each PageTimingMetricsSender is associated
  // with a distinct page navigation. Because we reset the object on commit,
  // we can trash last_timing_ on a provisional load before SendNow() fires.
  if (!last_timing_->navigation_start.is_null() &&
      last_timing_->navigation_start != timing->navigation_start) {
    return;
  }

  // We want to force sending the metrics quickly when some loading milestones
  // are reached (currently parse start, DCL, and FCP) so that the browser can
  // receive the accurate number of events. This accuracy is important to
  // measure the abandoned navigation.
  const bool send_urgently = IsFirstFCP(last_timing_, timing) ||
                             IsFirstParseStart(last_timing_, timing) ||
                             IsFirstDCL(last_timing_, timing);

  last_timing_ = std::move(timing);
  metadata_recorder_.UpdateMetadata(monotonic_timing);
  EnsureSendTimer(send_urgently);
}

void PageTimingMetricsSender::UpdateSoftNavigationMetrics(
    mojom::SoftNavigationMetricsPtr soft_navigation_metrics) {
  if (soft_navigation_metrics_->Equals(*soft_navigation_metrics)) {
    return;
  }

  soft_navigation_metrics_ = std::move(soft_navigation_metrics);

  EnsureSendTimer(true);
}

void PageTimingMetricsSender::SendCustomUserTimingMark(
    mojom::CustomUserTimingMarkPtr custom_timing) {
  // `custom_timing` is sent to the browser to clarify when the abandoned
  // navigation happens. When the navigation is abandoned, the renderer may be
  // busy, so it's important to start IPC and report UMA immediately.
  CHECK(custom_timing);
  sender_->SendCustomUserTiming(std::move(custom_timing));
}

void PageTimingMetricsSender::SendLatest() {
  if (!timer_->IsRunning())
    return;

  timer_->Stop();
  SendNow();
}

void PageTimingMetricsSender::UpdateCpuTiming(base::TimeDelta task_time) {
  last_cpu_timing_->task_time += task_time;
  EnsureSendTimer();
}

void PageTimingMetricsSender::EnsureSendTimer(bool urgent) {
  if (urgent)
    timer_->Stop();
  else if (timer_->IsRunning())
    return;

  int delay_ms;
  if (urgent) {
    // Send as soon as possible, but not synchronously, so that all pending
    // presentation callbacks for the current frame can run first.
    delay_ms = 0;
  } else if (have_sent_ipc_) {
    // This is the typical case.
    delay_ms = buffer_timer_delay_ms_;
  } else {
    // Send the first IPC eagerly to make sure the receiving side knows we're
    // sending metrics as soon as possible.
    delay_ms = kInitialTimerDelayMillis;
  }

  timer_->Start(FROM_HERE, base::Milliseconds(delay_ms),
                base::BindOnce(&PageTimingMetricsSender::SendNow,
                               base::Unretained(this)));
}

void PageTimingMetricsSender::SendNow() {
  have_sent_ipc_ = true;
  std::vector<mojom::ResourceDataUpdatePtr> resources;
  for (PageResourceDataUse* resource : modified_resources_) {
    resources.push_back(resource->GetResourceDataUpdate());
    if (resource->IsFinishedLoading()) {
      page_resource_data_use_.erase(resource->resource_id());
    }
  }

  sender_->SendTiming(last_timing_, metadata_, std::move(new_features_),
                      std::move(resources), render_data_, last_cpu_timing_,
                      std::move(input_timing_delta_), subresource_load_metrics_,
                      soft_navigation_metrics_);

  input_timing_delta_ = mojom::InputTiming::New();
  InitiateUserInteractionTiming();
  new_features_.clear();
  metadata_->main_frame_intersection_rect.reset();
  metadata_->main_frame_viewport_rect.reset();
  metadata_->main_frame_image_ad_rects.clear();
  last_cpu_timing_->task_time = base::TimeDelta();
  modified_resources_.clear();
  render_data_.new_layout_shifts.clear();
  render_data_.layout_shift_delta = 0;
  render_data_.layout_shift_delta_before_input_or_scroll = 0;
  // As PageTimingMetricsSender is owned by MetricsRenderFrameObserver, which is
  // instantiated for each frame, there's no need to make soft_navigation_count_
  // zero here, as its value only increments through the lifetime of the frame.
}

PageResourceDataUse* PageTimingMetricsSender::FindOrInsertPageResourceDataUse(
    int resource_id) {
  auto& data_use = page_resource_data_use_[resource_id];
  if (!data_use) {
    data_use = std::make_unique<PageResourceDataUse>(resource_id);
  }
  return data_use.get();
}

void PageTimingMetricsSender::InitiateUserInteractionTiming() {
  input_timing_delta_->max_event_durations =
      mojom::UserInteractionLatencies::NewUserInteractionLatencies({});
}

void PageTimingMetricsSender::DidObserveUserInteraction(
    base::TimeTicks max_event_start,
    base::TimeTicks max_event_queued_main_thread,
    base::TimeTicks max_event_commit_finish,
    base::TimeTicks max_event_end,
    blink::UserInteractionType interaction_type,
    uint64_t interaction_offset) {
  input_timing_delta_->num_interactions++;
  metadata_recorder_.AddInteractionDurationMetadata(max_event_start,
                                                    max_event_end);
  metadata_recorder_.AddInteractionDurationAfterQueueingMetadata(
      max_event_start, max_event_queued_main_thread, max_event_commit_finish,
      max_event_end);
  base::TimeDelta max_event_duration = max_event_end - max_event_start;
  input_timing_delta_->max_event_durations->get_user_interaction_latencies()
      .emplace_back(mojom::UserInteractionLatency::New(
          max_event_duration, UserInteractionTypeForMojom(interaction_type),
          interaction_offset, max_event_start));
  EnsureSendTimer();
}
}  // namespace page_load_metrics
