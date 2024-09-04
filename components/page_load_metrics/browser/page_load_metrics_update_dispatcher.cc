// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_update_dispatcher.h"

#include <optional>
#include <ostream>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "components/page_load_metrics/browser/layout_shift_normalization.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_interface.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace internal {

const char kPageLoadTimingStatus[] = "PageLoad.Internal.PageLoadTimingStatus";
const char kPageLoadTimingDispatchStatus[] =
    "PageLoad.Internal.PageLoadTimingStatus.AtTimingCallbackDispatch";
const char kPageLoadTimingPrerenderStatus[] =
    "PageLoad.Internal.PageLoadTimingStatus.OnPrerenderPage";
const char kPageLoadTimingFencedFramesStatus[] =
    "PageLoad.Internal.PageLoadTimingStatus.OnFencedFramesPage";

}  // namespace internal

namespace {

// Helper to allow use of Optional<> values in LOG() messages.
std::ostream& operator<<(std::ostream& os,
                         const std::optional<base::TimeDelta>& opt) {
  if (opt)
    os << opt.value();
  else
    os << "(unset)";
  return os;
}

// If second is non-zero, first must also be non-zero and less than or equal to
// second.
bool EventsInOrder(const std::optional<base::TimeDelta>& first,
                   const std::optional<base::TimeDelta>& second) {
  if (!second) {
    return true;
  }
  return first && first <= second;
}

internal::PageLoadTimingStatus IsValidPageLoadTiming(
    const mojom::PageLoadTiming& timing) {
  if (page_load_metrics::IsEmpty(timing))
    return internal::INVALID_EMPTY_TIMING;

  // If we have a non-empty timing, it should always have a navigation start.
  if (timing.navigation_start.is_null()) {
    LOG(ERROR) << "Received null navigation_start.";
    return internal::INVALID_NULL_NAVIGATION_START;
  }

  // Verify proper ordering between the various timings.

  // Note for activation_start
  //
  // PaintTiming is composed in MetricsRenderFrameObserver::GetTiming,
  // which also clamps wall clocks as navigation_start origin.
  // Majority of wall clocks are taken in render side, but
  // activation_start is taken in browser side
  // PageImpl::ActivateForPrerendering. Besides, there is no control
  // of these events. Therefore, we don't have any order relations
  // between activation_start and others except for navigation_start.
  // (Always 0 = navigation_start <= activation_start for main frames as
  // navigation_start origin TimeDelta.)

  if (!EventsInOrder(timing.response_start, timing.parse_timing->parse_start)) {
    // We sometimes get a zero response_start with a non-zero parse start. See
    // crbug.com/590212.
    LOG(ERROR) << "Invalid response_start " << timing.response_start
               << " for parse_start " << timing.parse_timing->parse_start;
    // When browser-side navigation is enabled, we sometimes encounter this
    // error case. For now, we disable reporting of this error, since most
    // PageLoadMetricsObservers don't care about response_start and we want to
    // see how much closer fixing this error will get us to page load metrics
    // being consistent with and without browser side navigation enabled. See
    // crbug.com/716587 for more details.
    //
    // return internal::INVALID_ORDER_RESPONSE_START_PARSE_START;
  }

  if (!EventsInOrder(timing.parse_timing->parse_start,
                     timing.parse_timing->parse_stop)) {
    LOG(ERROR) << "Invalid parse_start " << timing.parse_timing->parse_start
               << " for parse_stop " << timing.parse_timing->parse_stop;
    return internal::INVALID_ORDER_PARSE_START_PARSE_STOP;
  }

  if (timing.parse_timing->parse_stop) {
    const base::TimeDelta parse_duration =
        timing.parse_timing->parse_stop.value() -
        timing.parse_timing->parse_start.value();
    if (timing.parse_timing->parse_blocked_on_script_load_duration >
        parse_duration) {
      LOG(ERROR) << "Invalid parse_blocked_on_script_load_duration "
                 << timing.parse_timing->parse_blocked_on_script_load_duration
                 << " for parse duration " << parse_duration;
      return internal::INVALID_SCRIPT_LOAD_LONGER_THAN_PARSE;
    }
    if (timing.parse_timing->parse_blocked_on_script_execution_duration >
        parse_duration) {
      LOG(ERROR)
          << "Invalid parse_blocked_on_script_execution_duration "
          << timing.parse_timing->parse_blocked_on_script_execution_duration
          << " for parse duration " << parse_duration;
      return internal::INVALID_SCRIPT_EXEC_LONGER_THAN_PARSE;
    }
  }

  if (timing.parse_timing
          ->parse_blocked_on_script_load_from_document_write_duration >
      timing.parse_timing->parse_blocked_on_script_load_duration) {
    LOG(ERROR)
        << "Invalid parse_blocked_on_script_load_from_document_write_duration "
        << timing.parse_timing
               ->parse_blocked_on_script_load_from_document_write_duration
        << " for parse_blocked_on_script_load_duration "
        << timing.parse_timing->parse_blocked_on_script_load_duration;
    return internal::INVALID_SCRIPT_LOAD_DOC_WRITE_LONGER_THAN_SCRIPT_LOAD;
  }

  if (timing.parse_timing
          ->parse_blocked_on_script_execution_from_document_write_duration >
      timing.parse_timing->parse_blocked_on_script_execution_duration) {
    LOG(ERROR)
        << "Invalid "
           "parse_blocked_on_script_execution_from_document_write_duration "
        << timing.parse_timing
               ->parse_blocked_on_script_execution_from_document_write_duration
        << " for parse_blocked_on_script_execution_duration "
        << timing.parse_timing->parse_blocked_on_script_execution_duration;
    return internal::INVALID_SCRIPT_EXEC_DOC_WRITE_LONGER_THAN_SCRIPT_EXEC;
  }

  if (!EventsInOrder(timing.parse_timing->parse_stop,
                     timing.document_timing->dom_content_loaded_event_start)) {
    LOG(ERROR) << "Invalid parse_stop " << timing.parse_timing->parse_stop
               << " for dom_content_loaded_event_start "
               << timing.document_timing->dom_content_loaded_event_start;
    return internal::INVALID_ORDER_PARSE_STOP_DOM_CONTENT_LOADED;
  }

  if (!EventsInOrder(timing.document_timing->dom_content_loaded_event_start,
                     timing.document_timing->load_event_start)) {
    LOG(ERROR) << "Invalid dom_content_loaded_event_start "
               << timing.document_timing->dom_content_loaded_event_start
               << " for load_event_start "
               << timing.document_timing->load_event_start;
    return internal::INVALID_ORDER_DOM_CONTENT_LOADED_LOAD;
  }

  if (!EventsInOrder(timing.parse_timing->parse_start,
                     timing.paint_timing->first_paint)) {
    LOG(ERROR) << "Invalid parse_start " << timing.parse_timing->parse_start
               << " for first_paint " << timing.paint_timing->first_paint;
    return internal::INVALID_ORDER_PARSE_START_FIRST_PAINT;
  }

  if (!EventsInOrder(timing.paint_timing->first_paint,
                     timing.paint_timing->first_image_paint)) {
    LOG(ERROR) << "Invalid first_paint " << timing.paint_timing->first_paint
               << " for first_image_paint "
               << timing.paint_timing->first_image_paint;
    return internal::INVALID_ORDER_FIRST_PAINT_FIRST_IMAGE_PAINT;
  }

  if (!EventsInOrder(timing.paint_timing->first_paint,
                     timing.paint_timing->first_contentful_paint)) {
    LOG(ERROR) << "Invalid first_paint " << timing.paint_timing->first_paint
               << " for first_contentful_paint "
               << timing.paint_timing->first_contentful_paint;
    return internal::INVALID_ORDER_FIRST_PAINT_FIRST_CONTENTFUL_PAINT;
  }

  if (!EventsInOrder(timing.paint_timing->first_paint,
                     timing.paint_timing->first_meaningful_paint)) {
    LOG(ERROR) << "Invalid first_paint " << timing.paint_timing->first_paint
               << " for first_meaningful_paint "
               << timing.paint_timing->first_meaningful_paint;
    return internal::INVALID_ORDER_FIRST_PAINT_FIRST_MEANINGFUL_PAINT;
  }

  if (timing.interactive_timing->first_input_delay.has_value() &&
      !timing.interactive_timing->first_input_timestamp.has_value()) {
    return internal::INVALID_NULL_FIRST_INPUT_TIMESTAMP;
  }

  if (!timing.interactive_timing->first_input_delay.has_value() &&
      timing.interactive_timing->first_input_timestamp.has_value()) {
    return internal::INVALID_NULL_FIRST_INPUT_DELAY;
  }

  if (timing.interactive_timing->first_scroll_delay.has_value() &&
      !timing.interactive_timing->first_scroll_timestamp.has_value()) {
    return internal::INVALID_NULL_FIRST_SCROLL_TIMESTAMP;
  }

  if (!timing.interactive_timing->first_scroll_delay.has_value() &&
      timing.interactive_timing->first_scroll_timestamp.has_value()) {
    return internal::INVALID_NULL_FIRST_SCROLL_DELAY;
  }

  return internal::VALID;
}

// PageLoadTimingMerger merges timing values received from different frames
// together.
class PageLoadTimingMerger {
 public:
  explicit PageLoadTimingMerger(mojom::PageLoadTiming* target)
      : target_(target) {}

  PageLoadTimingMerger(const PageLoadTimingMerger&) = delete;
  PageLoadTimingMerger& operator=(const PageLoadTimingMerger&) = delete;

  // Merge timing values from |new_page_load_timing| into the target
  // PageLoadTiming;
  void Merge(base::TimeDelta navigation_start_offset,
             const mojom::PageLoadTiming& new_page_load_timing,
             bool is_main_frame) {
    MergePaintTiming(navigation_start_offset,
                     *new_page_load_timing.paint_timing, is_main_frame);
    MergeInteractiveTiming(navigation_start_offset,
                           *new_page_load_timing.interactive_timing,
                           is_main_frame);
    MergeBackForwardCacheTiming(navigation_start_offset,
                                new_page_load_timing.back_forward_cache_timings,
                                is_main_frame);
    if (is_main_frame) {
      MaybeUpdateTimeDelta(&target_->activation_start, navigation_start_offset,
                           new_page_load_timing.activation_start);
    }
  }

  // Whether we merged a new value.
  bool should_buffer_timing_update_callback() const {
    return should_buffer_timing_update_callback_;
  }

 private:
  // Updates *|inout_existing_value| with |optional_candidate_new_value|, if
  // either *|inout_existing_value| isn't set, or |optional_candidate_new_value|
  // < |inout_existing_value|. Set should_buffer_timing_update_callback_ to true
  // if a new value was merged. Returns true if an update occurred. Note that
  // |inout_existing_value| is relative to the main frame's navigation start.
  // |navigation_start_offset| contains the delta in navigation start time
  // between the main frame and the frame for |optional_candidate_new_value|.
  bool MaybeUpdateTimeDelta(
      std::optional<base::TimeDelta>* inout_existing_value,
      base::TimeDelta navigation_start_offset,
      const std::optional<base::TimeDelta>& optional_candidate_new_value) {
    // If we don't get a new value, there's nothing to do
    if (!optional_candidate_new_value)
      return false;

    // optional_candidate_new_value is relative to navigation start in its
    // frame. We need to adjust it to be relative to navigation start in the
    // main frame, so offset it by navigation_start_offset.
    base::TimeDelta candidate_new_value =
        navigation_start_offset + optional_candidate_new_value.value();

    DCHECK_NE(nullptr, inout_existing_value);
    if (inout_existing_value->has_value()) {
      // If we have a new value, but it is after the existing value, then keep
      // the existing value.
      if (*inout_existing_value <= candidate_new_value)
        return false;

      // We received a new timing event, but with a timestamp before the
      // timestamp of a timing update we had received previously. We expect this
      // to happen occasionally, as inter-frame updates can arrive out of order.
      // Record a histogram to track how frequently it happens, along with the
      // magnitude of the delta.
    } else {
      // We only want to set this for new updates. If there's already a value,
      // then the window during which we buffer updates is over. We'll still
      // update the value.
      // TODO(crbug.com/40562705): should we just throw the data out if we're
      // past the buffering window?
      should_buffer_timing_update_callback_ = true;
    }

    *inout_existing_value = candidate_new_value;
    return true;
  }

  // Merge paint timing values from |new_paint_timing| into the target
  // PaintTiming.
  void MergePaintTiming(base::TimeDelta navigation_start_offset,
                        const mojom::PaintTiming& new_paint_timing,
                        bool is_main_frame) {
    mojom::PaintTiming* target_paint_timing = target_->paint_timing.get();
    MaybeUpdateTimeDelta(&target_paint_timing->first_paint,
                         navigation_start_offset, new_paint_timing.first_paint);
    MaybeUpdateTimeDelta(&target_paint_timing->first_eligible_to_paint,
                         navigation_start_offset,
                         new_paint_timing.first_eligible_to_paint);
    MaybeUpdateTimeDelta(&target_paint_timing->first_image_paint,
                         navigation_start_offset,
                         new_paint_timing.first_image_paint);
    MaybeUpdateTimeDelta(&target_paint_timing->first_contentful_paint,
                         navigation_start_offset,
                         new_paint_timing.first_contentful_paint);
    if (is_main_frame) {
      // FMP is only tracked in the main frame.
      target_paint_timing->first_meaningful_paint =
          new_paint_timing.first_meaningful_paint;

      // LCP and the first input/scroll timestamp are not merged by us; instead,
      // PLMUD passes the per-frame data to its client (PageLoadTracker) via
      // OnTimingChanged and OnSubFrameTimingChanged, and merged results are
      // calculated and held by the LargestContentfulPaintHandler.
      target_paint_timing->largest_contentful_paint =
          new_paint_timing.largest_contentful_paint->Clone();
      target_paint_timing->experimental_largest_contentful_paint =
          new_paint_timing.experimental_largest_contentful_paint.Clone();
      target_paint_timing->first_input_or_scroll_notified_timestamp =
          new_paint_timing.first_input_or_scroll_notified_timestamp;
    }
  }

  void MergeInteractiveTiming(
      base::TimeDelta navigation_start_offset,
      const mojom::InteractiveTiming& new_interactive_timing,
      bool is_main_frame) {
    mojom::InteractiveTiming* target_interactive_timing =
        target_->interactive_timing.get();

    if (MaybeUpdateTimeDelta(&target_interactive_timing->first_input_timestamp,
                             navigation_start_offset,
                             new_interactive_timing.first_input_timestamp)) {
      // If we updated the first input timestamp, also update the
      // associated first input delay.
      target_interactive_timing->first_input_delay =
          new_interactive_timing.first_input_delay;
    }

    // Update First Scroll Delay.
    if (MaybeUpdateTimeDelta(&target_interactive_timing->first_scroll_timestamp,
                             navigation_start_offset,
                             new_interactive_timing.first_scroll_timestamp)) {
      target_interactive_timing->first_scroll_delay =
          new_interactive_timing.first_scroll_delay;
    }
  }

  void MergeBackForwardCacheTiming(
      base::TimeDelta navigation_start_offset,
      const std::vector<mojo::StructPtr<mojom::BackForwardCacheTiming>>&
          new_back_forward_cache_timings,
      bool is_main_frame) {
    if (is_main_frame) {
      target_->back_forward_cache_timings.clear();
      target_->back_forward_cache_timings.reserve(
          new_back_forward_cache_timings.size());
      for (const auto& timing : new_back_forward_cache_timings)
        target_->back_forward_cache_timings.push_back(timing.Clone());
    }
  }

  // The target PageLoadTiming we are merging values into.
  const raw_ptr<mojom::PageLoadTiming> target_;

  // Whether we merged a new value into |target_|.
  bool should_buffer_timing_update_callback_ = false;
};

}  // namespace

PageLoadMetricsUpdateDispatcher::PageLoadMetricsUpdateDispatcher(
    PageLoadMetricsUpdateDispatcher::Client* client,
    content::NavigationHandle* navigation_handle,
    PageLoadMetricsEmbedderInterface* embedder_interface)
    : client_(client),
      embedder_interface_(embedder_interface),
      timer_(embedder_interface->CreateTimer()),
      navigation_start_(navigation_handle->NavigationStart()),
      current_merged_page_timing_(CreatePageLoadTiming()),
      pending_merged_page_timing_(CreatePageLoadTiming()),
      main_frame_metadata_(mojom::FrameMetadata::New()),
      subframe_metadata_(mojom::FrameMetadata::New()),
      page_input_timing_(mojom::InputTiming::New()),
      is_prerendered_page_load_(navigation_handle->IsInPrerenderedMainFrame()) {
}

PageLoadMetricsUpdateDispatcher::~PageLoadMetricsUpdateDispatcher() {
  ShutDown();
}

void PageLoadMetricsUpdateDispatcher::ShutDown() {
  bool should_dispatch = false;
  if (timer_ && timer_->IsRunning()) {
    timer_->Stop();
    should_dispatch = true;
  }
  timer_ = nullptr;

  if (should_dispatch) {
    DispatchTimingUpdates();
  }
}

void PageLoadMetricsUpdateDispatcher::UpdateMetrics(
    content::RenderFrameHost* render_frame_host,
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
    internal::PageLoadTrackerPageType page_type) {
  if (embedder_interface_->IsExtensionUrl(
          render_frame_host->GetLastCommittedURL())) {
    // Extensions can inject child frames into a page. We don't want to track
    // these as they could skew metrics. See http://crbug.com/761037
    return;
  }

  // Report cpu usage.
  UpdateFrameCpuTiming(render_frame_host, std::move(new_cpu_timing));
  // Report data usage before new timing and metadata for messages that have
  // both updates.
  client_->UpdateResourceDataUse(render_frame_host, resources);

  UpdateHasSeenInputOrScroll(*new_timing);

  bool is_main_frame = client_->IsPageMainFrame(render_frame_host);
  if (is_main_frame) {
    UpdateMainFrameMetadata(render_frame_host, std::move(new_metadata));
    UpdateMainFrameTiming(std::move(new_timing), page_type);
    UpdateMainFrameRenderData(*render_data);
    if (subresource_load_metrics) {
      UpdateMainFrameSubresourceLoadMetrics(*subresource_load_metrics);
    }
    UpdateSoftNavigationIntervalResponsivenessMetrics(*input_timing_delta);
    UpdateSoftNavigationIntervalLayoutShift(*render_data);
    UpdateSoftNavigation(std::move(*soft_navigation_metrics));
  } else {
    if (!render_frame_host->GetParentOrOuterDocument()) {
      // TODO(crbug.com/40065854): `client_->IsPageMainFrame()` didn't return
      // the correct status.
      base::debug::DumpWithoutCrashing();
      return;
    }

    UpdateSubFrameMetadata(render_frame_host, std::move(new_metadata));
    UpdateSubFrameTiming(render_frame_host, std::move(new_timing));
    // This path is just for the AMP metrics.
    UpdateSubFrameInputTiming(render_frame_host, *input_timing_delta);
  }
  UpdatePageInputTiming(*input_timing_delta);
  UpdatePageRenderData(*render_data, is_main_frame);
  if (!is_main_frame) {
    // This path is just for the AMP metrics.
    OnSubFrameRenderDataChanged(render_frame_host, *render_data);
  }

  client_->UpdateFeaturesUsage(render_frame_host, new_features);
}

void PageLoadMetricsUpdateDispatcher::UpdateHasSeenInputOrScroll(
    const mojom::PageLoadTiming& new_timing) {
  const mojom::PaintTiming* paint_timing = new_timing.paint_timing.get();
  if (!paint_timing)
    return;

  // NOTE: we cannot use the first input/scroll in current_merged_page_timing_,
  // because PageLoadTimingMerger ignores this field.  We could reach into the
  // LargestContentfulPaintHandler, but it is simpler to just watch the
  // per-frame timing updates and remember a boolean.
  if (paint_timing->first_input_or_scroll_notified_timestamp.has_value())
    has_seen_input_or_scroll_ = true;
}

void PageLoadMetricsUpdateDispatcher::UpdateFeatures(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::UseCounterFeature>& new_features) {
  if (embedder_interface_->IsExtensionUrl(
          render_frame_host->GetLastCommittedURL())) {
    // Extensions can inject child frames into a page. We don't want to track
    // these as they could skew metrics. See http://crbug.com/761037
    return;
  }
  client_->UpdateFeaturesUsage(render_frame_host, new_features);
}

void PageLoadMetricsUpdateDispatcher::SetUpSharedMemoryForSmoothness(
    content::RenderFrameHost* render_frame_host,
    base::ReadOnlySharedMemoryRegion shared_memory) {
  const bool is_main_frame = client_->IsPageMainFrame(render_frame_host);
  if (is_main_frame) {
    client_->SetUpSharedMemoryForSmoothness(std::move(shared_memory));
  } else {
    // TODO(crbug.com/40144214): Merge smoothness metrics from OOPIFs with the
    // main-frame.
  }
}

void PageLoadMetricsUpdateDispatcher::DidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  // We have a new committed navigation, so discard information about the
  // previously committed navigation.
  subframe_navigation_start_offset_.erase(
      navigation_handle->GetFrameTreeNodeId());

  if (navigation_start_ > navigation_handle->NavigationStart()) {
    RecordInternalError(ERR_SUBFRAME_NAVIGATION_START_BEFORE_MAIN_FRAME);
    return;
  }
  base::TimeDelta navigation_delta =
      navigation_handle->NavigationStart() - navigation_start_;
  subframe_navigation_start_offset_.insert(std::make_pair(
      navigation_handle->GetFrameTreeNodeId(), navigation_delta));
}

void PageLoadMetricsUpdateDispatcher::OnSubFrameDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {
  subframe_navigation_start_offset_.erase(frame_tree_node_id);
}

void PageLoadMetricsUpdateDispatcher::UpdateSubFrameTiming(
    content::RenderFrameHost* render_frame_host,
    mojom::PageLoadTimingPtr new_timing) {
  const auto it = subframe_navigation_start_offset_.find(
      render_frame_host->GetFrameTreeNodeId());
  if (it == subframe_navigation_start_offset_.end()) {
    // We received timing information for an untracked load. Ignore it.
    return;
  }

  client_->OnSubFrameTimingChanged(render_frame_host, *new_timing);

  base::TimeDelta navigation_start_offset = it->second;
  PageLoadTimingMerger merger(pending_merged_page_timing_.get());
  merger.Merge(navigation_start_offset, *new_timing, false /* is_main_frame */);

  MaybeDispatchTimingUpdates(merger.should_buffer_timing_update_callback());
}

void PageLoadMetricsUpdateDispatcher::UpdateSubFrameInputTiming(
    content::RenderFrameHost* render_frame_host,
    const mojom::InputTiming& input_timing_delta) {
  client_->OnSubFrameInputTimingChanged(render_frame_host, input_timing_delta);
}

void PageLoadMetricsUpdateDispatcher::UpdateFrameCpuTiming(
    content::RenderFrameHost* render_frame_host,
    mojom::CpuTimingPtr new_timing) {
  // If the task time is zero, then there's nothing to do.
  if (new_timing->task_time.is_zero())
    return;
  // If this is not the main frame, make sure it's valid.
  if (render_frame_host->GetParent() != nullptr) {
    const auto it = subframe_navigation_start_offset_.find(
        render_frame_host->GetFrameTreeNodeId());
    if (it == subframe_navigation_start_offset_.end()) {
      // We received timing information for an untracked load. Ignore it.
      return;
    }
  }
  client_->UpdateFrameCpuTiming(render_frame_host, *new_timing);
}

void PageLoadMetricsUpdateDispatcher::UpdateSubFrameMetadata(
    content::RenderFrameHost* render_frame_host,
    mojom::FrameMetadataPtr subframe_metadata) {
  if (subframe_metadata->main_frame_viewport_rect) {
    mojo::ReportBadMessage(
        "Unexpected main_frame_viewport_rect set for a subframe.");
    return;
  }

  // Merge the subframe loading behavior flags with any we've already observed,
  // possibly from other subframes.
  subframe_metadata_->behavior_flags |= subframe_metadata->behavior_flags;
  client_->OnSubframeMetadataChanged(render_frame_host, *subframe_metadata);

  MaybeUpdateMainFrameIntersectionRect(render_frame_host, subframe_metadata);
}

void PageLoadMetricsUpdateDispatcher::UpdateMainFrameSubresourceLoadMetrics(
    const blink::SubresourceLoadMetrics& subresource_load_metrics) {
  subresource_load_metrics_ = subresource_load_metrics;
}

void PageLoadMetricsUpdateDispatcher::UpdateSoftNavigation(
    const mojom::SoftNavigationMetrics& soft_navigation_metrics) {
  client_->OnSoftNavigationChanged(soft_navigation_metrics);
}

void PageLoadMetricsUpdateDispatcher::UpdateSoftNavigationIntervalLayoutShift(
    const mojom::FrameRenderDataUpdate& render_data) {
  soft_nav_interval_render_data_.layout_shift_score +=
      render_data.layout_shift_delta;
  soft_nav_interval_layout_shift_normalization_.AddNewLayoutShifts(
      render_data.new_layout_shifts, base::TimeTicks::Now(),
      soft_nav_interval_render_data_.layout_shift_score);
}

void PageLoadMetricsUpdateDispatcher::
    UpdateSoftNavigationIntervalResponsivenessMetrics(
        const mojom::InputTiming& input_timing_delta) {
  if (input_timing_delta.num_interactions) {
    soft_navigation_interval_responsiveness_metrics_normalization_
        .AddNewUserInteractionLatencies(
            input_timing_delta.num_interactions,
            *(input_timing_delta.max_event_durations));
  }
}

void PageLoadMetricsUpdateDispatcher::MaybeUpdateMainFrameIntersectionRect(
    content::RenderFrameHost* render_frame_host,
    const mojom::FrameMetadataPtr& frame_metadata) {
  // Handle intersection updates if included in the metadata.
  if (!frame_metadata->main_frame_intersection_rect)
    return;

  // Do not notify intersections for untracked loads,
  // subframe_navigation_start_offset_ excludes untracked loads.
  // TODO(crbug.com/40679417): Document definition of untracked loads in page
  // load metrics.
  const content::FrameTreeNodeId frame_tree_node_id =
      render_frame_host->GetFrameTreeNodeId();
  bool is_main_frame = client_->IsPageMainFrame(render_frame_host);
  if (!is_main_frame &&
      subframe_navigation_start_offset_.find(frame_tree_node_id) ==
          subframe_navigation_start_offset_.end()) {
    return;
  }

  auto existing_intersection_it =
      main_frame_intersection_rects_.find(frame_tree_node_id);

  // Check if we already have a frame intersection rect for the frame, dispatch
  // updates for the first frame intersection rect or if the intersection has
  // changed.
  if (existing_intersection_it == main_frame_intersection_rects_.end() ||
      existing_intersection_it->second !=
          *frame_metadata->main_frame_intersection_rect) {
    main_frame_intersection_rects_[frame_tree_node_id] =
        *frame_metadata->main_frame_intersection_rect;
    client_->OnMainFrameIntersectionRectChanged(
        render_frame_host, *frame_metadata->main_frame_intersection_rect);
  }
}

void PageLoadMetricsUpdateDispatcher::MaybeUpdateMainFrameViewportRect(
    const mojom::FrameMetadataPtr& frame_metadata) {
  // Handle viewport updates if included in the metadata.
  if (!frame_metadata->main_frame_viewport_rect)
    return;

  if (!main_frame_viewport_rect_ ||
      *frame_metadata->main_frame_viewport_rect != *main_frame_viewport_rect_) {
    main_frame_viewport_rect_ = *frame_metadata->main_frame_viewport_rect;
    client_->OnMainFrameViewportRectChanged(*main_frame_viewport_rect_);
  }
}

void PageLoadMetricsUpdateDispatcher::UpdateMainFrameTiming(
    mojom::PageLoadTimingPtr new_timing,
    internal::PageLoadTrackerPageType page_type) {
  // Throw away IPCs that are not relevant to the current navigation.
  // Two timing structures cannot refer to the same navigation if they indicate
  // that a navigation started at different times, so a new timing struct with a
  // different start time from an earlier struct is considered invalid.
  const bool valid_timing_descendent =
      pending_merged_page_timing_->navigation_start.is_null() ||
      pending_merged_page_timing_->navigation_start ==
          new_timing->navigation_start;
  if (!valid_timing_descendent) {
    RecordInternalError(ERR_BAD_TIMING_IPC_INVALID_TIMING_DESCENDENT);
    return;
  }

  internal::PageLoadTimingStatus status = IsValidPageLoadTiming(*new_timing);
  base::UmaHistogramEnumeration(internal::kPageLoadTimingStatus, status,
                                internal::LAST_PAGE_LOAD_TIMING_STATUS);
  if (page_type == internal::PageLoadTrackerPageType::kPrerenderPage) {
    base::UmaHistogramEnumeration(internal::kPageLoadTimingPrerenderStatus,
                                  status,
                                  internal::LAST_PAGE_LOAD_TIMING_STATUS);
  } else if (page_type ==
             internal::PageLoadTrackerPageType::kFencedFramesPage) {
    base::UmaHistogramEnumeration(internal::kPageLoadTimingFencedFramesStatus,
                                  status,
                                  internal::LAST_PAGE_LOAD_TIMING_STATUS);
  }
  if (status != internal::VALID) {
    RecordInternalError(ERR_BAD_TIMING_IPC_INVALID_TIMING);
    return;
  }

  mojom::PaintTimingPtr last_paint_timing =
      std::move(pending_merged_page_timing_->paint_timing);

  mojom::InteractiveTimingPtr last_interactive_timing =
      std::move(pending_merged_page_timing_->interactive_timing);

  // Update the pending_merged_page_timing_, making sure to merge the previously
  // observed |paint_timing| and |interactive_timing|, which are tracked across
  // all frames in the page.
  pending_merged_page_timing_ = new_timing->Clone();
  pending_merged_page_timing_->paint_timing = std::move(last_paint_timing);
  pending_merged_page_timing_->interactive_timing =
      std::move(last_interactive_timing);

  PageLoadTimingMerger merger(pending_merged_page_timing_.get());
  merger.Merge(base::TimeDelta(), *new_timing, true /* is_main_frame */);
  MaybeDispatchTimingUpdates(merger.should_buffer_timing_update_callback());
}

void PageLoadMetricsUpdateDispatcher::UpdateMainFrameMetadata(
    content::RenderFrameHost* render_frame_host,
    mojom::FrameMetadataPtr new_metadata) {
  if (main_frame_metadata_->Equals(*new_metadata))
    return;

  // Ensure flags sent previously are still present in the new metadata fields.
  const bool valid_behavior_descendent =
      (main_frame_metadata_->behavior_flags & new_metadata->behavior_flags) ==
      main_frame_metadata_->behavior_flags;
  if (!valid_behavior_descendent) {
    RecordInternalError(ERR_BAD_TIMING_IPC_INVALID_BEHAVIOR_DESCENDENT);
    return;
  }

  main_frame_metadata_ = std::move(new_metadata);
  client_->OnMainFrameMetadataChanged();

  if (!main_frame_metadata_.is_null()) {
    MaybeUpdateMainFrameIntersectionRect(render_frame_host,
                                         main_frame_metadata_);
    MaybeUpdateMainFrameViewportRect(main_frame_metadata_);

    client_->OnMainFrameImageAdRectsChanged(
        main_frame_metadata_->main_frame_image_ad_rects);
  }
}

void PageLoadMetricsUpdateDispatcher::UpdatePageInputTiming(
    const mojom::InputTiming& input_timing_delta) {
  // On the sending side, we ensure input_timing_delta.max_event_duration and
  // input_timing_delta.total_event_durations are not null pointers otherwise
  // VALIDATION_ERROR_UNEXPECTED_NULL_POINTER will be triggered on the receiving
  // side. But in some tests where the whole input_timing_delta is set as the
  // default state, input_timing_delta.max_event_durations or
  // input_timing_delta.total_event_durations can be null.
  if (input_timing_delta.num_interactions) {
    responsiveness_metrics_normalization_.AddNewUserInteractionLatencies(
        input_timing_delta.num_interactions,
        *(input_timing_delta.max_event_durations));
  }
  if (input_timing_delta.num_interactions) {
    client_->OnPageInputTimingChanged(input_timing_delta.num_interactions);
  }
}

void PageLoadMetricsUpdateDispatcher::UpdatePageRenderData(
    const mojom::FrameRenderDataUpdate& render_data,
    bool is_main_frame) {
  page_render_data_.layout_shift_score += render_data.layout_shift_delta;
  layout_shift_normalization_.AddNewLayoutShifts(
      render_data.new_layout_shifts, base::TimeTicks::Now(),
      page_render_data_.layout_shift_score);
  layout_shift_normalization_for_bfcache_.AddNewLayoutShifts(
      render_data.new_layout_shifts, base::TimeTicks::Now(),
      page_render_data_.layout_shift_score -
          cumulative_layout_shift_score_for_bfcache_);

  // Stop accumulating page-wide layout_shift_score_before_input_or_scroll after
  // input or scroll in any frame. Note that we can't unconditionally accumulate
  // layout_shift_delta_before_input_or_scroll, because that field only reflects
  // input/scroll in the same frame as the shift.
  if (!has_seen_input_or_scroll_) {
    page_render_data_.layout_shift_score_before_input_or_scroll +=
        render_data.layout_shift_delta_before_input_or_scroll;
  }

  client_->OnPageRenderDataChanged(render_data, is_main_frame);
}

void PageLoadMetricsUpdateDispatcher::UpdateMainFrameRenderData(
    const mojom::FrameRenderDataUpdate& render_data) {
  main_frame_render_data_.layout_shift_score += render_data.layout_shift_delta;

  // Track main frame cumulative score up to the first input or scroll in the
  // main frame. For this we do not care about inputs sent to subframes, so we
  // should not check has_seen_input_or_scroll_ (but see crbug.com/1136207).
  main_frame_render_data_.layout_shift_score_before_input_or_scroll +=
      render_data.layout_shift_delta_before_input_or_scroll;
}

void PageLoadMetricsUpdateDispatcher::OnSubFrameRenderDataChanged(
    content::RenderFrameHost* render_frame_host,
    const mojom::FrameRenderDataUpdate& render_data) {
  client_->OnSubFrameRenderDataChanged(render_frame_host, render_data);
}

void PageLoadMetricsUpdateDispatcher::FlushPendingTimingUpdates() {
  // If there's a pending update, dispatch the update now.
  if (timer_->IsRunning()) {
    timer_->Stop();
    DispatchTimingUpdates();
  }
}

void PageLoadMetricsUpdateDispatcher::MaybeDispatchTimingUpdates(
    bool should_buffer_timing_update_callback) {
  // If we merged a new timing value, then we should buffer updates to allow for
  // any other out of order timings to arrive before we dispatch to observers.
  if (should_buffer_timing_update_callback) {
    timer_->Start(
        FROM_HERE,
        base::Milliseconds(GetBufferTimerDelayMillis(TimerType::kBrowser)),
        base::BindOnce(&PageLoadMetricsUpdateDispatcher::DispatchTimingUpdates,
                       base::Unretained(this)));
  } else if (!timer_->IsRunning()) {
    DispatchTimingUpdates();
  }
}

void PageLoadMetricsUpdateDispatcher::DispatchTimingUpdates() {
  if (pending_merged_page_timing_->paint_timing->first_paint) {
    if (!pending_merged_page_timing_->parse_timing->parse_start) {
      // When merging paint events across frames, we can sometimes encounter
      // cases where we've received a first paint event for a child frame before
      // receiving required earlier events in the main frame, due to buffering
      // in the render process which results in out of order delivery. For
      // example, we may receive a notification for a first paint in a child
      // frame before we've received a notification for parse start in the main
      // frame. In these cases, we delay sending timing updates until we've
      // received all expected events (e.g. wait to receive a parse event before
      // dispatching a paint event), so observers can make assumptions about
      // ordering of these events in their callbacks.
      return;
    }
    if (is_prerendered_page_load_ &&
        !pending_merged_page_timing_->activation_start) {
      // Similarly, in a prerendered page load we may receive a first paint in a
      // child frame before we've received a notification for activation start
      // in the main frame.
      return;
    }
  }
  if (current_merged_page_timing_->Equals(*pending_merged_page_timing_))
    return;

  current_merged_page_timing_ = pending_merged_page_timing_->Clone();

  internal::PageLoadTimingStatus status =
      IsValidPageLoadTiming(*pending_merged_page_timing_);
  base::UmaHistogramEnumeration(internal::kPageLoadTimingDispatchStatus, status,
                                internal::LAST_PAGE_LOAD_TIMING_STATUS);

  client_->OnTimingChanged();
}

}  // namespace page_load_metrics
