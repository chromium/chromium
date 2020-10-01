// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_update_dispatcher.h"

#include <ostream>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_interface.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace internal {

const char kPageLoadTimingStatus[] = "PageLoad.Internal.PageLoadTimingStatus";
const char kPageLoadTimingDispatchStatus[] =
    "PageLoad.Internal.PageLoadTimingStatus.AtTimingCallbackDispatch";
const char kHistogramOutOfOrderTiming[] =
    "PageLoad.Internal.OutOfOrderInterFrameTiming";
const char kHistogramOutOfOrderTimingBuffered[] =
    "PageLoad.Internal.OutOfOrderInterFrameTiming.AfterBuffering";

}  // namespace internal

namespace {

// Helper to allow use of Optional<> values in LOG() messages.
std::ostream& operator<<(std::ostream& os,
                         const base::Optional<base::TimeDelta>& opt) {
  if (opt)
    os << opt.value();
  else
    os << "(unset)";
  return os;
}

// If second is non-zero, first must also be non-zero and less than or equal to
// second.
bool EventsInOrder(const base::Optional<base::TimeDelta>& first,
                   const base::Optional<base::TimeDelta>& second) {
  if (!second) {
    return true;
  }
  return first && first <= second;
}

bool IsMainFrame(content::RenderFrameHost* render_frame_host) {
  return render_frame_host->GetParent() == nullptr;
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

  if (timing.interactive_timing->longest_input_delay.has_value() &&
      !timing.interactive_timing->longest_input_timestamp.has_value()) {
    return internal::INVALID_NULL_LONGEST_INPUT_TIMESTAMP;
  }

  if (!timing.interactive_timing->longest_input_delay.has_value() &&
      timing.interactive_timing->longest_input_timestamp.has_value()) {
    return internal::INVALID_NULL_LONGEST_INPUT_DELAY;
  }

  if (timing.interactive_timing->longest_input_delay.has_value() &&
      timing.interactive_timing->first_input_delay.has_value() &&
      timing.interactive_timing->longest_input_delay <
          timing.interactive_timing->first_input_delay) {
    return internal::INVALID_LONGEST_INPUT_DELAY_LESS_THAN_FIRST_INPUT_DELAY;
  }

  if (timing.interactive_timing->longest_input_timestamp.has_value() &&
      timing.interactive_timing->first_input_timestamp.has_value() &&
      timing.interactive_timing->longest_input_timestamp <
          timing.interactive_timing->first_input_timestamp) {
    return internal::
        INVALID_LONGEST_INPUT_TIMESTAMP_LESS_THAN_FIRST_INPUT_TIMESTAMP;
  }

  return internal::VALID;
}

// If the updated value has an earlier time than the current value, log so we
// can keep track of how often this happens.
void LogIfOutOfOrderTiming(const base::Optional<base::TimeDelta>& current,
                           const base::Optional<base::TimeDelta>& update) {
  if (!current || !update)
    return;

  if (update < current) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramOutOfOrderTimingBuffered,
                        current.value() - update.value());
  }
}

// PageLoadTimingMerger merges timing values received from different frames
// together.
class PageLoadTimingMerger {
 public:
  explicit PageLoadTimingMerger(mojom::PageLoadTiming* target)
      : target_(target) {}

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
      base::Optional<base::TimeDelta>* inout_existing_value,
      base::TimeDelta navigation_start_offset,
      const base::Optional<base::TimeDelta>& optional_candidate_new_value) {
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
      PAGE_LOAD_HISTOGRAM(internal::kHistogramOutOfOrderTiming,
                          inout_existing_value->value() - candidate_new_value);
    } else {
      // We only want to set this for new updates. If there's already a value,
      // then the window during which we buffer updates is over. We'll still
      // update the value.
      // TODO(811752): should we just throw the data out if we're past the
      // buffering window?
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
    MaybeUpdateTimeDelta(&target_paint_timing->first_image_paint,
                         navigation_start_offset,
                         new_paint_timing.first_image_paint);
    MaybeUpdateTimeDelta(&target_paint_timing->first_contentful_paint,
                         navigation_start_offset,
                         new_paint_timing.first_contentful_paint);
    if (is_main_frame) {
      // FMP and FCP++ are only tracked in the main frame.
      target_paint_timing->first_meaningful_paint =
          new_paint_timing.first_meaningful_paint;

      target_paint_timing->largest_contentful_paint =
          new_paint_timing.largest_contentful_paint->Clone();
      target_paint_timing->experimental_largest_contentful_paint =
          new_paint_timing.experimental_largest_contentful_paint.Clone();
      target_paint_timing->first_input_or_scroll_notified_timestamp =
          new_paint_timing.first_input_or_scroll_notified_timestamp;
      target_paint_timing->portal_activated_paint =
          new_paint_timing.portal_activated_paint;
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
      if (new_interactive_timing.first_input_processing_time.has_value()) {
        target_interactive_timing->first_input_processing_time =
            new_interactive_timing.first_input_processing_time;
      }
    }

    if (new_interactive_timing.longest_input_delay.has_value()) {
      base::TimeDelta new_longest_input_timestamp =
          navigation_start_offset +
          new_interactive_timing.longest_input_timestamp.value();
      if (!target_interactive_timing->longest_input_delay.has_value() ||
          new_interactive_timing.longest_input_delay.value() >
              target_interactive_timing->longest_input_delay.value()) {
        target_interactive_timing->longest_input_delay =
            new_interactive_timing.longest_input_delay;
        target_interactive_timing->longest_input_timestamp =
            new_longest_input_timestamp;
      }
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
  mojom::PageLoadTiming* const target_;

  // Whether we merged a new value into |target_|.
  bool should_buffer_timing_update_callback_ = false;

  DISALLOW_COPY_AND_ASSIGN(PageLoadTimingMerger);
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
      page_input_timing_(mojom::InputTiming()) {}

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
    mojom::PageLoadFeaturesPtr new_features,
    const std::vector<mojom::ResourceDataUpdatePtr>& resources,
    mojom::FrameRenderDataUpdatePtr render_data,
    mojom::CpuTimingPtr new_cpu_timing,
    mojom::DeferredResourceCountsPtr new_deferred_resource_data,
    mojom::InputTimingPtr input_timing_delta) {
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

  // Report new deferral info.
  client_->OnNewDeferredResourceCounts(*new_deferred_resource_data);

  bool is_main_frame = IsMainFrame(render_frame_host);
  if (is_main_frame) {
    UpdateMainFrameMetadata(render_frame_host, std::move(new_metadata));
    UpdateMainFrameTiming(std::move(new_timing));
    UpdateMainFrameRenderData(*render_data);
  } else {
    UpdateSubFrameMetadata(render_frame_host, std::move(new_metadata));
    UpdateSubFrameTiming(render_frame_host, std::move(new_timing));
  }
  UpdatePageInputTiming(*input_timing_delta);
  UpdatePageRenderData(*render_data);
  if (!is_main_frame) {
    // This path is just for the AMP metrics.
    OnSubFrameRenderDataChanged(render_frame_host, *render_data);
  }

  client_->UpdateFeaturesUsage(render_frame_host, *new_features);
}

void PageLoadMetricsUpdateDispatcher::UpdateFeatures(
    content::RenderFrameHost* render_frame_host,
    const mojom::PageLoadFeatures& new_features) {
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
  const bool is_main_frame = IsMainFrame(render_frame_host);
  if (is_main_frame) {
    client_->SetUpSharedMemoryForSmoothness(std::move(shared_memory));
  } else {
    // TODO(1115136): Merge smoothness metrics from OOPIFs with the main-frame.
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

void PageLoadMetricsUpdateDispatcher::OnFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  subframe_navigation_start_offset_.erase(
      render_frame_host->GetFrameTreeNodeId());
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
  // Merge the subframe loading behavior flags with any we've already observed,
  // possibly from other subframes.
  subframe_metadata_->behavior_flags |= subframe_metadata->behavior_flags;
  client_->OnSubframeMetadataChanged(render_frame_host, *subframe_metadata);

  MaybeUpdateFrameIntersection(render_frame_host, subframe_metadata);
}

void PageLoadMetricsUpdateDispatcher::MaybeUpdateFrameIntersection(
    content::RenderFrameHost* render_frame_host,
    const mojom::FrameMetadataPtr& frame_metadata) {
  // Handle intersection updates if included in the metadata.
  if (frame_metadata->intersection_update.is_null())
    return;

  // Do not notify intersections for untracked loads,
  // subframe_navigation_start_offset_ excludes untracked loads.
  // TODO(crbug/1061091): Document definition of untracked loads in page load
  // metrics.
  const int frame_tree_node_id = render_frame_host->GetFrameTreeNodeId();
  bool is_main_frame = IsMainFrame(render_frame_host);
  if (!is_main_frame &&
      subframe_navigation_start_offset_.find(frame_tree_node_id) ==
          subframe_navigation_start_offset_.end()) {
    return;
  }

  auto existing_intersection_it =
      frame_intersection_updates_.find(frame_tree_node_id);

  // Check if we already have a frame intersection update for the frame,
  // dispatch updates for the first frame intersection update or if
  // the intersection has changed.
  if (existing_intersection_it == frame_intersection_updates_.end() ||
      !existing_intersection_it->second.Equals(
          *frame_metadata->intersection_update)) {
    frame_intersection_updates_[frame_tree_node_id] =
        *frame_metadata->intersection_update;
    client_->OnFrameIntersectionUpdate(render_frame_host,
                                       *frame_metadata->intersection_update);
  }
}

void PageLoadMetricsUpdateDispatcher::UpdateMainFrameTiming(
    mojom::PageLoadTimingPtr new_timing) {
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
  UMA_HISTOGRAM_ENUMERATION(internal::kPageLoadTimingStatus, status,
                            internal::LAST_PAGE_LOAD_TIMING_STATUS);
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

  if (!main_frame_metadata_.is_null())
    MaybeUpdateFrameIntersection(render_frame_host, main_frame_metadata_);
}

void PageLoadMetricsUpdateDispatcher::UpdatePageInputTiming(
    const mojom::InputTiming& input_timing_delta) {
  page_input_timing_.num_input_events += input_timing_delta.num_input_events;
  page_input_timing_.total_input_delay += input_timing_delta.total_input_delay;
  page_input_timing_.total_adjusted_input_delay +=
      input_timing_delta.total_adjusted_input_delay;
}

void PageLoadMetricsUpdateDispatcher::UpdatePageRenderData(
    const mojom::FrameRenderDataUpdate& render_data) {
  page_render_data_.layout_shift_score += render_data.layout_shift_delta;

  page_render_data_.all_layout_block_count +=
      render_data.all_layout_block_count_delta;
  page_render_data_.ng_layout_block_count +=
      render_data.ng_layout_block_count_delta;
  page_render_data_.all_layout_call_count +=
      render_data.all_layout_call_count_delta;
  page_render_data_.ng_layout_call_count +=
      render_data.ng_layout_call_count_delta;
}

void PageLoadMetricsUpdateDispatcher::UpdateMainFrameRenderData(
    const mojom::FrameRenderDataUpdate& render_data) {
  main_frame_render_data_.layout_shift_score += render_data.layout_shift_delta;
  main_frame_render_data_.layout_shift_score_before_input_or_scroll +=
      render_data.layout_shift_delta_before_input_or_scroll;

  main_frame_render_data_.all_layout_block_count +=
      render_data.all_layout_block_count_delta;
  main_frame_render_data_.ng_layout_block_count +=
      render_data.ng_layout_block_count_delta;
  main_frame_render_data_.all_layout_call_count +=
      render_data.all_layout_call_count_delta;
  main_frame_render_data_.ng_layout_call_count +=
      render_data.ng_layout_call_count_delta;
}

void PageLoadMetricsUpdateDispatcher::OnSubFrameRenderDataChanged(
    content::RenderFrameHost* render_frame_host,
    const mojom::FrameRenderDataUpdate& render_data) {
  client_->OnSubFrameRenderDataChanged(render_frame_host, render_data);
}

void PageLoadMetricsUpdateDispatcher::MaybeDispatchTimingUpdates(
    bool should_buffer_timing_update_callback) {
  // If we merged a new timing value, then we should buffer updates for
  // |kBufferTimerDelayMillis|, to allow for any other out of order timings to
  // arrive before we dispatch the minimum observed timings to observers.
  if (should_buffer_timing_update_callback) {
    timer_->Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kBufferTimerDelayMillis),
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
  }
  if (current_merged_page_timing_->Equals(*pending_merged_page_timing_))
    return;

  LogIfOutOfOrderTiming(current_merged_page_timing_->paint_timing->first_paint,
                        pending_merged_page_timing_->paint_timing->first_paint);
  LogIfOutOfOrderTiming(
      current_merged_page_timing_->paint_timing->first_image_paint,
      pending_merged_page_timing_->paint_timing->first_image_paint);
  LogIfOutOfOrderTiming(
      current_merged_page_timing_->paint_timing->first_contentful_paint,
      pending_merged_page_timing_->paint_timing->first_contentful_paint);

  current_merged_page_timing_ = pending_merged_page_timing_->Clone();

  internal::PageLoadTimingStatus status =
      IsValidPageLoadTiming(*pending_merged_page_timing_);
  UMA_HISTOGRAM_ENUMERATION(internal::kPageLoadTimingDispatchStatus, status,
                            internal::LAST_PAGE_LOAD_TIMING_STATUS);

  client_->OnTimingChanged();
}

}  // namespace page_load_metrics
