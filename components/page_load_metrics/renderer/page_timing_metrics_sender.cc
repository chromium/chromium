// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_timing_metrics_sender.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/page_load_metrics/common/page_load_metrics_constants.h"
#include "components/page_load_metrics/renderer/page_timing_sender.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace page_load_metrics {

namespace {
const int kInitialTimerDelayMillis = 50;
const int64_t kInputDelayAdjustmentMillis = int64_t(50);
const base::Feature kPageLoadMetricsTimerDelayFeature{
    "PageLoadMetricsTimerDelay", base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace

PageTimingMetricsSender::PageTimingMetricsSender(
    std::unique_ptr<PageTimingSender> sender,
    std::unique_ptr<base::OneShotTimer> timer,
    mojom::PageLoadTimingPtr initial_timing,
    const PageTimingMetadataRecorder::MonotonicTiming& initial_monotonic_timing,
    std::unique_ptr<PageResourceDataUse> initial_request)
    : sender_(std::move(sender)),
      timer_(std::move(timer)),
      last_timing_(std::move(initial_timing)),
      last_cpu_timing_(mojom::CpuTiming::New()),
      input_timing_delta_(mojom::InputTiming::New()),
      metadata_(mojom::FrameMetadata::New()),
      new_features_(mojom::PageLoadFeatures::New()),
      new_deferred_resource_data_(mojom::DeferredResourceCounts::New()),
      buffer_timer_delay_ms_(kBufferTimerDelayMillis),
      metadata_recorder_(initial_monotonic_timing) {
  const auto resource_id = initial_request->resource_id();
  page_resource_data_use_.emplace(
      std::piecewise_construct, std::forward_as_tuple(resource_id),
      std::forward_as_tuple(std::move(initial_request)));
  buffer_timer_delay_ms_ = base::GetFieldTrialParamByFeatureAsInt(
      kPageLoadMetricsTimerDelayFeature, "BufferTimerDelayMillis",
      kBufferTimerDelayMillis /* default value */);
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

void PageTimingMetricsSender::DidObserveNewFeatureUsage(
    blink::mojom::WebFeature feature) {
  size_t feature_id = static_cast<size_t>(feature);
  if (features_sent_.test(feature_id)) {
    return;
  }
  features_sent_.set(feature_id);
  new_features_->features.push_back(feature);
  EnsureSendTimer();
}

void PageTimingMetricsSender::DidObserveNewCssPropertyUsage(
    blink::mojom::CSSSampleId css_property,
    bool is_animated) {
  size_t css_property_id = static_cast<size_t>(css_property);
  if (is_animated && !animated_css_properties_sent_.test(css_property_id)) {
    animated_css_properties_sent_.set(css_property_id);
    new_features_->animated_css_properties.push_back(css_property);
    EnsureSendTimer();
  } else if (!is_animated && !css_properties_sent_.test(css_property_id)) {
    css_properties_sent_.set(css_property_id);
    new_features_->css_properties.push_back(css_property);
    EnsureSendTimer();
  }
}

void PageTimingMetricsSender::DidObserveLayoutShift(
    double score,
    bool after_input_or_scroll) {
  DCHECK(score > 0);
  render_data_.layout_shift_delta += score;
  if (!after_input_or_scroll)
    render_data_.layout_shift_delta_before_input_or_scroll += score;
  EnsureSendTimer();
}

void PageTimingMetricsSender::DidObserveLayoutNg(uint32_t all_block_count,
                                                 uint32_t ng_block_count,
                                                 uint32_t all_call_count,
                                                 uint32_t ng_call_count) {
  render_data_.all_layout_block_count_delta += all_block_count;
  render_data_.ng_layout_block_count_delta += ng_block_count;
  render_data_.all_layout_call_count_delta += all_call_count;
  render_data_.ng_layout_call_count_delta += ng_call_count;
  EnsureSendTimer();
}

void PageTimingMetricsSender::DidObserveLazyLoadBehavior(
    blink::WebLocalFrameClient::LazyLoadBehavior lazy_load_behavior) {
  switch (lazy_load_behavior) {
    case blink::WebLocalFrameClient::LazyLoadBehavior::kDeferredFrame:
      ++new_deferred_resource_data_->deferred_frames;
      break;
    case blink::WebLocalFrameClient::LazyLoadBehavior::kDeferredImage:
      ++new_deferred_resource_data_->deferred_images;
      break;
    case blink::WebLocalFrameClient::LazyLoadBehavior::kLazyLoadedFrame:
      ++new_deferred_resource_data_->frames_loaded_after_deferral;
      break;
    case blink::WebLocalFrameClient::LazyLoadBehavior::kLazyLoadedImage:
      ++new_deferred_resource_data_->images_loaded_after_deferral;
      break;
  }
}

void PageTimingMetricsSender::DidStartResponse(
    const GURL& response_url,
    int resource_id,
    const network::mojom::URLResponseHead& response_head,
    network::mojom::RequestDestination request_destination) {
  DCHECK(!base::Contains(page_resource_data_use_, resource_id));

  auto resource_it = page_resource_data_use_.emplace(
      std::piecewise_construct, std::forward_as_tuple(resource_id),
      std::forward_as_tuple(std::make_unique<PageResourceDataUse>()));
  resource_it.first->second->DidStartResponse(
      response_url, resource_id, response_head, request_destination);
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
  auto resource_it = page_resource_data_use_.find(resource_id);

  // It is possible that resources are not in the map, if response headers were
  // not received or for failed/cancelled resources. For data reduction proxy
  // purposes treat these as having no savings.
  if (resource_it == page_resource_data_use_.end()) {
    auto new_resource_it = page_resource_data_use_.emplace(
        std::piecewise_construct, std::forward_as_tuple(resource_id),
        std::forward_as_tuple(std::make_unique<PageResourceDataUse>()));
    resource_it = new_resource_it.first;
  }

  resource_it->second->DidCompleteResponse(status);
  EnsureSendTimer();
  modified_resources_.insert(resource_it->second.get());
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

  auto resource_it = page_resource_data_use_.emplace(
      std::piecewise_construct, std::forward_as_tuple(request_id),
      std::forward_as_tuple(std::make_unique<PageResourceDataUse>()));
  resource_it.first->second->DidLoadFromMemoryCache(
      response_url, request_id, encoded_body_length, mime_type);
  modified_resources_.insert(resource_it.first->second.get());
}

void PageTimingMetricsSender::OnMainFrameIntersectionChanged(
    const blink::WebRect& main_frame_intersection) {
  metadata_->intersection_update =
      mojom::FrameIntersectionUpdate::New(gfx::Rect(main_frame_intersection));
  EnsureSendTimer();
}

void PageTimingMetricsSender::UpdateResourceMetadata(
    int resource_id,
    bool reported_as_ad_resource,
    bool is_main_frame_resource,
    bool completed_before_fcp) {
  auto it = page_resource_data_use_.find(resource_id);
  if (it == page_resource_data_use_.end())
    return;

  // This can get called multiple times for a resource, and this flag will only
  // be true once.
  if (reported_as_ad_resource)
    it->second->SetReportedAsAdResource(reported_as_ad_resource);

  // This can get called multiple times for a resource, and this flag will only
  // be true once.
  if (completed_before_fcp)
    it->second->SetCompletedBeforeFCP(completed_before_fcp);

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

  last_timing_ = std::move(timing);
  metadata_recorder_.UpdateMetadata(monotonic_timing);
  EnsureSendTimer();
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

void PageTimingMetricsSender::EnsureSendTimer() {
  if (timer_->IsRunning())
    return;

  // Send the first IPC eagerly to make sure the receiving side knows we're
  // sending metrics as soon as possible.
  int delay_ms =
      have_sent_ipc_ ? buffer_timer_delay_ms_ : kInitialTimerDelayMillis;
  timer_->Start(FROM_HERE, base::TimeDelta::FromMilliseconds(delay_ms),
                base::BindOnce(&PageTimingMetricsSender::SendNow,
                               base::Unretained(this)));
}

void PageTimingMetricsSender::SendNow() {
  have_sent_ipc_ = true;
  std::vector<mojom::ResourceDataUpdatePtr> resources;
  for (auto* resource : modified_resources_) {
    resources.push_back(resource->GetResourceDataUpdate());
    if (resource->IsFinishedLoading()) {
      page_resource_data_use_.erase(resource->resource_id());
    }
  }

  sender_->SendTiming(last_timing_, metadata_, std::move(new_features_),
                      std::move(resources), render_data_, last_cpu_timing_,
                      std::move(new_deferred_resource_data_),
                      std::move(input_timing_delta_));
  input_timing_delta_ = mojom::InputTiming::New();
  new_deferred_resource_data_ = mojom::DeferredResourceCounts::New();
  new_features_ = mojom::PageLoadFeatures::New();
  metadata_->intersection_update.reset();
  last_cpu_timing_->task_time = base::TimeDelta();
  modified_resources_.clear();
  render_data_.layout_shift_delta = 0;
  render_data_.layout_shift_delta_before_input_or_scroll = 0;
  render_data_.all_layout_block_count_delta = 0;
  render_data_.ng_layout_block_count_delta = 0;
  render_data_.all_layout_call_count_delta = 0;
  render_data_.ng_layout_call_count_delta = 0;
}

void PageTimingMetricsSender::DidObserveInputDelay(
    base::TimeDelta input_delay) {
  input_timing_delta_->num_input_events++;
  input_timing_delta_->total_input_delay += input_delay;
  input_timing_delta_->total_adjusted_input_delay +=
      base::TimeDelta::FromMilliseconds(
          std::max(int64_t(0),
                   input_delay.InMilliseconds() - kInputDelayAdjustmentMillis));
  EnsureSendTimer();
}

}  // namespace page_load_metrics
