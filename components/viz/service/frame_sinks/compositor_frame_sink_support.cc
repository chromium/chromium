// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/power_scheduler/power_mode.h"
#include "components/power_scheduler/power_mode_arbiter.h"
#include "components/power_scheduler/power_mode_voter.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_reference.h"
#include "components/viz/service/transitions/surface_animation_manager.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace viz {
namespace {

void RecordShouldSendBeginFrame(const std::string& reason) {
  TRACE_EVENT1("viz", "ShouldNotSendBeginFrame", "reason", reason);
}

void AdjustPresentationFeedback(gfx::PresentationFeedback* feedback,
                                base::TimeTicks swap_start) {
  // Swap start to end breakdown is always reported if ready timestamp is
  // available. The other timestamps are adjusted to assume 0 delay in those
  // stages if the breakdown is not available.
  if (feedback->ready_timestamp.is_null())
    return;

  feedback->available_timestamp =
      std::max(feedback->available_timestamp, swap_start);
  feedback->latch_timestamp =
      std::max(feedback->latch_timestamp, feedback->ready_timestamp);
}

}  // namespace

CompositorFrameSinkSupport::CompositorFrameSinkSupport(
    mojom::CompositorFrameSinkClient* client,
    FrameSinkManagerImpl* frame_sink_manager,
    const FrameSinkId& frame_sink_id,
    bool is_root)
    : client_(client),
      frame_sink_manager_(frame_sink_manager),
      surface_manager_(frame_sink_manager->surface_manager()),
      frame_sink_id_(frame_sink_id),
      surface_resource_holder_(this),
      is_root_(is_root),
      allow_copy_output_requests_(is_root),
      animation_power_mode_voter_(
          power_scheduler::PowerModeArbiter::GetInstance()->NewVoter(
              "PowerModeVoter.Animation")) {
  // This may result in SetBeginFrameSource() being called.
  frame_sink_manager_->RegisterCompositorFrameSinkSupport(frame_sink_id_, this);
}

CompositorFrameSinkSupport::~CompositorFrameSinkSupport() {
  // Unregister |this| as a BeginFrameObserver so that the
  // BeginFrameSource does not call into |this| after it's deleted.
  callback_received_begin_frame_ = true;
  callback_received_receive_ack_ = true;
  frame_timing_details_.clear();
  SetNeedsBeginFrame(false);

  // For display root surfaces the surface is no longer going to be visible.
  // Make it unreachable from the top-level root.
  if (referenced_local_surface_id_.has_value()) {
    auto reference = MakeTopLevelRootReference(
        SurfaceId(frame_sink_id_, referenced_local_surface_id_.value()));
    surface_manager_->RemoveSurfaceReferences({reference});
  }

  if (last_activated_surface_id_.is_valid())
    EvictLastActiveSurface();
  if (last_created_surface_id_.is_valid())
    surface_manager_->MarkSurfaceForDestruction(last_created_surface_id_);
  frame_sink_manager_->UnregisterCompositorFrameSinkSupport(frame_sink_id_);

  // The display compositor has ownership of shared memory for each
  // SharedBitmapId that has been reported from the client. Since the client is
  // gone that memory can be freed. If we don't then it would leak.
  for (const auto& id : owned_bitmaps_)
    frame_sink_manager_->shared_bitmap_manager()->ChildDeletedSharedBitmap(id);

  // No video capture clients should remain after calling
  // UnregisterCompositorFrameSinkSupport().
  DCHECK(capture_clients_.empty());

  if (begin_frame_source_ && added_frame_observer_)
    begin_frame_source_->RemoveObserver(this);
}

FrameTimingDetailsMap CompositorFrameSinkSupport::TakeFrameTimingDetailsMap() {
  FrameTimingDetailsMap map;
  map.swap(frame_timing_details_);
  return map;
}

void CompositorFrameSinkSupport::SetUpHitTest(
    LatestLocalSurfaceIdLookupDelegate* local_surface_id_lookup_delegate) {
  DCHECK(is_root_);
  hit_test_aggregator_ = std::make_unique<HitTestAggregator>(
      frame_sink_manager_->hit_test_manager(), frame_sink_manager_,
      local_surface_id_lookup_delegate, frame_sink_id_);
}

void CompositorFrameSinkSupport::SetAggregatedDamageCallbackForTesting(
    AggregatedDamageCallback callback) {
  aggregated_damage_callback_ = std::move(callback);
}

void CompositorFrameSinkSupport::SetBeginFrameSource(
    BeginFrameSource* begin_frame_source) {
  if (begin_frame_source_ && added_frame_observer_) {
    begin_frame_source_->RemoveObserver(this);
    added_frame_observer_ = false;
  }
  begin_frame_source_ = begin_frame_source;
  UpdateNeedsBeginFramesInternal();
}

void CompositorFrameSinkSupport::ThrottleBeginFrame(base::TimeDelta interval) {
  begin_frame_interval_ = interval;
}

void CompositorFrameSinkSupport::OnSurfaceActivated(Surface* surface) {
  DCHECK(surface);
  DCHECK(surface->HasActiveFrame());
  DCHECK(!surface->HasPendingFrame());

  pending_surfaces_.erase(surface);
  if (pending_surfaces_.empty())
    UpdateNeedsBeginFramesInternal();

  // Let the animation manager process any new directives on the surface.
  // TODO(vmpstr): Figure out if `last_frame_time_` is correct here.
  // SurfaceAcitvation may have happened some time after we sent the last
  // BeginFrame to the client (which is when the frame time is updated). We may
  // need to keep track of a separate time that updates when OnBeginFrame
  // happens whether or not it is sent to the client.
  bool started_animation =
      surface_animation_manager_.ProcessTransitionDirectives(
          last_frame_time_,
          surface->GetActiveFrameMetadata().transition_directives,
          surface->GetSurfaceSavedFrameStorage());
  // If processing the new directives caused us to start an animation, then
  // interpoate the frame immediately. This is needed since if we wait until the
  // next BeginFrame to do the first interpolation, then we maybe have already
  // drawn this destination frame.
  if (started_animation)
    surface_animation_manager_.InterpolateFrame(surface);

  // The above call can cause us to start an animation, meaning we need begin
  // frames. If that's the case, make sure to update the begin frame
  // observation.
  // TODO(vmpstr): Note that if we need to produce an interpolation to the
  // latest frame, then this is where we would do that.
  if (surface_animation_manager_.NeedsBeginFrame())
    UpdateNeedsBeginFramesInternal();

  if (surface->surface_id() == last_activated_surface_id_)
    return;

  Surface* previous_surface =
      surface_manager_->GetSurfaceForId(last_activated_surface_id_);

  if (!previous_surface) {
    last_activated_surface_id_ = surface->surface_id();
  } else if (previous_surface->GetActiveFrameIndex() <
             surface->GetActiveFrameIndex()) {
    surface_manager_->MarkSurfaceForDestruction(last_activated_surface_id_);
    last_activated_surface_id_ = surface->surface_id();
    // TODO(samans): Why is this not done when creating the surface?
    surface->SetPreviousFrameSurface(previous_surface);
  } else {
    DCHECK_GT(previous_surface->GetActiveFrameIndex(),
              surface->GetActiveFrameIndex());
    // We can get into a situation where a child-initiated synchronization is
    // deferred until after a parent-initiated synchronization happens resulting
    // in activations happening out of order. In that case, we simply discard
    // the stale surface.
    surface_manager_->MarkSurfaceForDestruction(surface->surface_id());
  }

  // Check if this is a display root surface and the SurfaceId is changing.
  if (is_root_ && (!referenced_local_surface_id_ ||
                   *referenced_local_surface_id_ !=
                       last_activated_surface_id_.local_surface_id())) {
    UpdateDisplayRootReference(surface);
  }

  MaybeEvictSurfaces();
}

void CompositorFrameSinkSupport::OnSurfaceWillDraw(Surface* surface) {
  if (last_drawn_frame_index_ >= surface->GetActiveFrameIndex())
    return;
  last_drawn_frame_index_ = surface->GetActiveFrameIndex();
}

void CompositorFrameSinkSupport::OnFrameTokenChanged(uint32_t frame_token) {
  frame_sink_manager_->OnFrameTokenChanged(frame_sink_id_, frame_token);
}

void CompositorFrameSinkSupport::OnSurfaceProcessed(Surface* surface) {
  DidReceiveCompositorFrameAck();
}

void CompositorFrameSinkSupport::OnSurfaceAggregatedDamage(
    Surface* surface,
    const LocalSurfaceId& local_surface_id,
    const CompositorFrame& frame,
    const gfx::Rect& damage_rect,
    base::TimeTicks expected_display_time) {
  DCHECK(!damage_rect.IsEmpty());

  const gfx::Size& frame_size_in_pixels = frame.size_in_pixels();
  if (aggregated_damage_callback_) {
    aggregated_damage_callback_.Run(local_surface_id, frame_size_in_pixels,
                                    damage_rect, expected_display_time);
  }

  for (CapturableFrameSink::Client* client : capture_clients_) {
    client->OnFrameDamaged(frame_size_in_pixels, damage_rect,
                           expected_display_time, frame.metadata);
  }
}

void CompositorFrameSinkSupport::OnSurfaceDestroyed(Surface* surface) {
  pending_surfaces_.erase(surface);

  if (surface->surface_id() == last_activated_surface_id_)
    last_activated_surface_id_ = SurfaceId();

  if (surface->surface_id() == last_created_surface_id_)
    last_created_surface_id_ = SurfaceId();
}

void CompositorFrameSinkSupport::OnSurfacePresented(
    uint32_t frame_token,
    base::TimeTicks draw_start_timestamp,
    const gfx::SwapTimings& swap_timings,
    const gfx::PresentationFeedback& feedback) {
  DidPresentCompositorFrame(frame_token, draw_start_timestamp, swap_timings,
                            feedback);
}

void CompositorFrameSinkSupport::RefResources(
    const std::vector<TransferableResource>& resources) {
  surface_animation_manager_.RefResources(resources);
  surface_resource_holder_.RefResources(resources);
}

void CompositorFrameSinkSupport::UnrefResources(
    const std::vector<ReturnedResource>& resources) {
  surface_animation_manager_.UnrefResources(resources);
  surface_resource_holder_.UnrefResources(resources);
}

void CompositorFrameSinkSupport::ReturnResources(
    const std::vector<ReturnedResource>& resources) {
  if (resources.empty())
    return;
  if (!ack_pending_count_ && client_) {
    client_->ReclaimResources(resources);
    return;
  }

  std::copy(resources.begin(), resources.end(),
            std::back_inserter(surface_returned_resources_));
}

void CompositorFrameSinkSupport::ReceiveFromChild(
    const std::vector<TransferableResource>& resources) {
  surface_resource_holder_.ReceiveFromChild(resources);
}

std::vector<PendingCopyOutputRequest>
CompositorFrameSinkSupport::TakeCopyOutputRequests(
    const LocalSurfaceId& latest_local_id) {
  std::vector<PendingCopyOutputRequest> results;
  for (auto it = copy_output_requests_.begin();
       it != copy_output_requests_.end();) {
    // Requests with a non-valid local id should be satisfied as soon as
    // possible.
    if (!it->local_surface_id.is_valid() ||
        it->local_surface_id <= latest_local_id) {
      results.push_back(std::move(*it));
      it = copy_output_requests_.erase(it);
    } else {
      ++it;
    }
  }
  return results;
}

void CompositorFrameSinkSupport::EvictSurface(const LocalSurfaceId& id) {
  DCHECK(id.embed_token() != last_evicted_local_surface_id_.embed_token() ||
         id.parent_sequence_number() >=
             last_evicted_local_surface_id_.parent_sequence_number());
  last_evicted_local_surface_id_ = id;
  surface_manager_->DropTemporaryReference(SurfaceId(frame_sink_id_, id));
  MaybeEvictSurfaces();
}

void CompositorFrameSinkSupport::MaybeEvictSurfaces() {
  if (IsEvicted(last_activated_surface_id_.local_surface_id()))
    EvictLastActiveSurface();
  if (IsEvicted(last_created_surface_id_.local_surface_id())) {
    surface_manager_->MarkSurfaceForDestruction(last_created_surface_id_);
    last_created_surface_id_ = SurfaceId();
  }
}

void CompositorFrameSinkSupport::EvictLastActiveSurface() {
  SurfaceId to_destroy_surface_id = last_activated_surface_id_;
  if (last_created_surface_id_ == last_activated_surface_id_)
    last_created_surface_id_ = SurfaceId();
  last_activated_surface_id_ = SurfaceId();
  surface_manager_->MarkSurfaceForDestruction(to_destroy_surface_id);

  // For display root surfaces the surface is no longer going to be visible.
  // Make it unreachable from the top-level root.
  if (referenced_local_surface_id_.has_value()) {
    auto reference = MakeTopLevelRootReference(
        SurfaceId(frame_sink_id_, referenced_local_surface_id_.value()));
    surface_manager_->RemoveSurfaceReferences({reference});
    referenced_local_surface_id_.reset();
  }
}

void CompositorFrameSinkSupport::SetNeedsBeginFrame(bool needs_begin_frame) {
  client_needs_begin_frame_ = needs_begin_frame;
  UpdateNeedsBeginFramesInternal();
}

void CompositorFrameSinkSupport::SetWantsAnimateOnlyBeginFrames() {
  wants_animate_only_begin_frames_ = true;
}

bool CompositorFrameSinkSupport::WantsAnimateOnlyBeginFrames() const {
  return wants_animate_only_begin_frames_;
}

void CompositorFrameSinkSupport::InitializeCompositorFrameSinkType(
    mojom::CompositorFrameSinkType type) {
  if (frame_sink_type_ != mojom::CompositorFrameSinkType::kUnspecified ||
      type == mojom::CompositorFrameSinkType::kUnspecified) {
    return;
  }
  frame_sink_type_ = type;
}

base::TimeDelta CompositorFrameSinkSupport::GetPreferredFrameInterval(
    mojom::CompositorFrameSinkType* type) const {
  if (type)
    *type = frame_sink_type_;
  return preferred_frame_interval_;
}

bool CompositorFrameSinkSupport::IsRoot() const {
  return is_root_;
}

void CompositorFrameSinkSupport::DidNotProduceFrame(const BeginFrameAck& ack) {
  TRACE_EVENT_WITH_FLOW2(
      "viz,benchmark", "Graphics.Pipeline", TRACE_ID_GLOBAL(ack.trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "DidNotProduceFrame", "FrameSinkId", frame_sink_id_.ToString());
  DCHECK(ack.frame_id.IsSequenceValid());

  begin_frame_tracker_.ReceivedAck(ack);

  // Override the has_damage flag (ignoring invalid data from clients).
  BeginFrameAck modified_ack(ack);
  modified_ack.has_damage = false;

  if (last_activated_surface_id_.is_valid())
    surface_manager_->SurfaceModified(last_activated_surface_id_, modified_ack);

  if (begin_frame_source_) {
    begin_frame_source_->DidFinishFrame(this);
    frame_sink_manager_->DidFinishFrame(frame_sink_id_, last_begin_frame_args_);
  }
}

void CompositorFrameSinkSupport::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    base::Optional<HitTestRegionList> hit_test_region_list,
    uint64_t submit_time) {
  const auto result = MaybeSubmitCompositorFrame(
      local_surface_id, std::move(frame), std::move(hit_test_region_list),
      submit_time,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  DCHECK_EQ(result, SubmitResult::ACCEPTED);
}

bool CompositorFrameSinkSupport::DidAllocateSharedBitmap(
    base::ReadOnlySharedMemoryRegion region,
    const SharedBitmapId& id) {
  if (!frame_sink_manager_->shared_bitmap_manager()->ChildAllocatedSharedBitmap(
          region.Map(), id)) {
    return false;
  }

  owned_bitmaps_.insert(id);
  return true;
}

void CompositorFrameSinkSupport::DidDeleteSharedBitmap(
    const SharedBitmapId& id) {
  frame_sink_manager_->shared_bitmap_manager()->ChildDeletedSharedBitmap(id);
  owned_bitmaps_.erase(id);
}

SubmitResult CompositorFrameSinkSupport::MaybeSubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    base::Optional<HitTestRegionList> hit_test_region_list,
    uint64_t submit_time,
    mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback callback) {
  TRACE_EVENT_WITH_FLOW2(
      "viz,benchmark", "Graphics.Pipeline",
      TRACE_ID_GLOBAL(frame.metadata.begin_frame_ack.trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "ReceiveCompositorFrame", "FrameSinkId", frame_sink_id_.ToString());

  DCHECK(local_surface_id.is_valid());
  DCHECK(!frame.render_pass_list.empty());
  DCHECK(!frame.size_in_pixels().IsEmpty());

  CHECK(callback_received_begin_frame_);
  CHECK(callback_received_receive_ack_);

  begin_frame_tracker_.ReceivedAck(frame.metadata.begin_frame_ack);
  ++ack_pending_count_;

  compositor_frame_callback_ = std::move(callback);
  if (compositor_frame_callback_) {
    callback_received_begin_frame_ = false;
    callback_received_receive_ack_ = false;
    UpdateNeedsBeginFramesInternal();
  }

  base::TimeTicks now_time = base::TimeTicks::Now();
  pending_received_frame_times_.emplace(frame.metadata.frame_token, now_time);

  // Override the has_damage flag (ignoring invalid data from clients).
  frame.metadata.begin_frame_ack.has_damage = true;
  DCHECK(frame.metadata.begin_frame_ack.frame_id.IsSequenceValid());

  if (!ui::LatencyInfo::Verify(
          frame.metadata.latency_info,
          "CompositorFrameSinkSupport::MaybeSubmitCompositorFrame")) {
    for (auto& info : frame.metadata.latency_info) {
      info.Terminate();
    }
    std::vector<ui::LatencyInfo>().swap(frame.metadata.latency_info);
  }
  for (ui::LatencyInfo& latency : frame.metadata.latency_info) {
    if (latency.latency_components().size() > 0) {
      latency.AddLatencyNumberWithTimestamp(
          ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, now_time);
    }
  }

  base::ScopedClosureRunner frame_rejected_callback(
      base::BindOnce(&CompositorFrameSinkSupport::DidRejectCompositorFrame,
                     weak_factory_.GetWeakPtr(), frame.metadata.frame_token,
                     frame.resource_list, frame.metadata.latency_info));

  // Ensure no CopyOutputRequests have been submitted if they are banned.
  if (!allow_copy_output_requests_ && frame.HasCopyOutputRequests()) {
    TRACE_EVENT_INSTANT0("viz", "CopyOutputRequests not allowed",
                         TRACE_EVENT_SCOPE_THREAD);
    return SubmitResult::COPY_OUTPUT_REQUESTS_NOT_ALLOWED;
  }

  // TODO(crbug.com/846739): It should be possible to use
  // |frame.metadata.frame_token| instead of maintaining a |last_frame_index_|.
  uint64_t frame_index = ++last_frame_index_;

  if (frame.metadata.preferred_frame_interval)
    preferred_frame_interval_ = *frame.metadata.preferred_frame_interval;
  else
    preferred_frame_interval_ = BeginFrameArgs::MinInterval();

  Surface* prev_surface =
      surface_manager_->GetSurfaceForId(last_created_surface_id_);
  Surface* current_surface = nullptr;
  if (prev_surface &&
      local_surface_id == last_created_surface_id_.local_surface_id()) {
    current_surface = prev_surface;
  } else {
    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "LocalSurfaceId.Submission.Flow",
        TRACE_ID_GLOBAL(local_surface_id.submission_trace_id()),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
        "ReceiveCompositorFrame", "local_surface_id",
        local_surface_id.ToString());

    SurfaceId surface_id(frame_sink_id_, local_surface_id);
    SurfaceInfo surface_info(surface_id, frame.device_scale_factor(),
                             frame.size_in_pixels());

    // LocalSurfaceIds should be monotonically increasing. This ID is used
    // to determine the freshness of a surface at aggregation time.
    const LocalSurfaceId& last_created_local_surface_id =
        last_created_surface_id_.local_surface_id();

    bool child_initiated_synchronization_event =
        last_created_local_surface_id.is_valid() &&
        local_surface_id.child_sequence_number() >
            last_created_local_surface_id.child_sequence_number();

    // Neither sequence numbers of the LocalSurfaceId can decrease and at least
    // one must increase.
    bool monotonically_increasing_id =
        (local_surface_id.parent_sequence_number() >=
             last_created_local_surface_id.parent_sequence_number() &&
         local_surface_id.child_sequence_number() >=
             last_created_local_surface_id.child_sequence_number()) &&
        (local_surface_id.parent_sequence_number() >
             last_created_local_surface_id.parent_sequence_number() ||
         child_initiated_synchronization_event);

    DCHECK(surface_info.is_valid());
    if (local_surface_id.embed_token() ==
            last_created_local_surface_id.embed_token() &&
        !monotonically_increasing_id) {
      TRACE_EVENT_INSTANT0("viz", "LocalSurfaceId decreased",
                           TRACE_EVENT_SCOPE_THREAD);
      return SubmitResult::SURFACE_ID_DECREASED;
    }

    // Don't recreate a surface that was previously evicted. Drop the
    // CompositorFrame and return all its resources.
    if (IsEvicted(local_surface_id)) {
      TRACE_EVENT_INSTANT0("viz", "Submit rejected to evicted surface",
                           TRACE_EVENT_SCOPE_THREAD);
      return SubmitResult::ACCEPTED;
    }

    current_surface = surface_manager_->CreateSurface(
        weak_factory_.GetWeakPtr(), surface_info);
    if (!current_surface) {
      TRACE_EVENT_INSTANT0("viz", "Surface belongs to another client",
                           TRACE_EVENT_SCOPE_THREAD);
      return SubmitResult::SURFACE_OWNED_BY_ANOTHER_CLIENT;
    }
    last_created_surface_id_ = SurfaceId(frame_sink_id_, local_surface_id);

    surface_manager_->SurfaceDamageExpected(current_surface->surface_id(),
                                            last_begin_frame_args_);
  }

  const int64_t trace_id = ~frame.metadata.begin_frame_ack.trace_id;
  TRACE_EVENT_WITH_FLOW1(TRACE_DISABLED_BY_DEFAULT("viz.hit_testing_flow"),
                         "Event.Pipeline", TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "ReceiveHitTestData");

  // QueueFrame can fail in unit tests, so SubmitHitTestRegionList has to be
  // called before that.
  frame_sink_manager()->SubmitHitTestRegionList(
      last_created_surface_id_, frame_index, std::move(hit_test_region_list));

  Surface::QueueFrameResult result = current_surface->QueueFrame(
      std::move(frame), frame_index, std::move(frame_rejected_callback));
  switch (result) {
    case Surface::QueueFrameResult::REJECTED:
      TRACE_EVENT_INSTANT0("viz", "QueueFrame failed",
                           TRACE_EVENT_SCOPE_THREAD);
      return SubmitResult::SIZE_MISMATCH;
    case Surface::QueueFrameResult::ACCEPTED_PENDING:
      // Make sure we periodically check if the frame should activate.
      pending_surfaces_.insert(current_surface);
      UpdateNeedsBeginFramesInternal();
      break;
    case Surface::QueueFrameResult::ACCEPTED_ACTIVE:
      // Nothing to do here.
      break;
  }

  if (begin_frame_source_) {
    begin_frame_source_->DidFinishFrame(this);
    frame_sink_manager_->DidFinishFrame(frame_sink_id_, last_begin_frame_args_);
  }

  return SubmitResult::ACCEPTED;
}

SurfaceReference CompositorFrameSinkSupport::MakeTopLevelRootReference(
    const SurfaceId& surface_id) {
  return SurfaceReference(surface_manager_->GetRootSurfaceId(), surface_id);
}

void CompositorFrameSinkSupport::HandleCallback() {
  if (!compositor_frame_callback_ || !callback_received_begin_frame_ ||
      !callback_received_receive_ack_) {
    return;
  }

  std::move(compositor_frame_callback_)
      .Run(std::move(surface_returned_resources_));
  surface_returned_resources_.clear();
}

void CompositorFrameSinkSupport::DidReceiveCompositorFrameAck() {
  DCHECK_GT(ack_pending_count_, 0);
  ack_pending_count_--;
  if (!client_)
    return;

  // If we have a callback, we only return the resource on onBeginFrame.
  if (compositor_frame_callback_) {
    callback_received_receive_ack_ = true;
    UpdateNeedsBeginFramesInternal();
    HandleCallback();
    return;
  }

  client_->DidReceiveCompositorFrameAck(surface_returned_resources_);
  surface_returned_resources_.clear();
}

void CompositorFrameSinkSupport::DidPresentCompositorFrame(
    uint32_t frame_token,
    base::TimeTicks draw_start_timestamp,
    const gfx::SwapTimings& swap_timings,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(frame_token);
  DCHECK((feedback.flags & gfx::PresentationFeedback::kFailure) ||
         (!draw_start_timestamp.is_null() && !swap_timings.is_null()));

  DCHECK_LE(pending_received_frame_times_.size(), 25u);
  auto received_frame_timestamp =
      pending_received_frame_times_.find(frame_token);
  DCHECK(received_frame_timestamp != pending_received_frame_times_.end());

  FrameTimingDetails details;
  details.received_compositor_frame_timestamp =
      received_frame_timestamp->second;
  details.draw_start_timestamp = draw_start_timestamp;
  details.swap_timings = swap_timings;
  details.presentation_feedback = feedback;
  AdjustPresentationFeedback(&details.presentation_feedback,
                             swap_timings.swap_start);
  pending_received_frame_times_.erase(received_frame_timestamp);

  // We should only ever get one PresentationFeedback per frame_token.
  DCHECK(frame_timing_details_.find(frame_token) ==
         frame_timing_details_.end());
  frame_timing_details_.emplace(frame_token, details);

  UpdateNeedsBeginFramesInternal();
}

void CompositorFrameSinkSupport::DidRejectCompositorFrame(
    uint32_t frame_token,
    std::vector<TransferableResource> frame_resource_list,
    std::vector<ui::LatencyInfo> latency_info) {
  TRACE_EVENT_INSTANT0("viz", "DidRejectCompositorFrame",
                       TRACE_EVENT_SCOPE_THREAD);
  // TODO(eseckler): Should these be stored and attached to the next successful
  // frame submission instead?
  for (ui::LatencyInfo& info : latency_info)
    info.Terminate();

  std::vector<ReturnedResource> resources =
      TransferableResource::ReturnResources(frame_resource_list);
  ReturnResources(resources);
  DidReceiveCompositorFrameAck();
  DidPresentCompositorFrame(frame_token, base::TimeTicks(), gfx::SwapTimings(),
                            gfx::PresentationFeedback::Failure());
}

void CompositorFrameSinkSupport::UpdateDisplayRootReference(
    const Surface* surface) {
  // Make the new SurfaceId reachable from the top-level root.
  surface_manager_->AddSurfaceReferences(
      {MakeTopLevelRootReference(surface->surface_id())});

  // Make the old SurfaceId unreachable from the top-level root if applicable.
  if (referenced_local_surface_id_) {
    surface_manager_->RemoveSurfaceReferences({MakeTopLevelRootReference(
        SurfaceId(frame_sink_id_, *referenced_local_surface_id_))});
  }

  referenced_local_surface_id_ = surface->surface_id().local_surface_id();
}

void CompositorFrameSinkSupport::OnBeginFrame(const BeginFrameArgs& args) {
  if (compositor_frame_callback_) {
    callback_received_begin_frame_ = true;
    UpdateNeedsBeginFramesInternal();
    HandleCallback();
  }

  CheckPendingSurfaces();

  if (client_ && ShouldSendBeginFrame(args.frame_time)) {
    if (last_activated_surface_id_.is_valid())
      surface_manager_->SurfaceDamageExpected(last_activated_surface_id_, args);
    last_begin_frame_args_ = args;

    BeginFrameArgs copy_args = args;
    // Force full frame if surface not yet activated to ensure surface creation.
    if (!last_activated_surface_id_.is_valid())
      copy_args.animate_only = false;

    copy_args.trace_id = ComputeTraceId();
    TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                           TRACE_ID_GLOBAL(copy_args.trace_id),
                           TRACE_EVENT_FLAG_FLOW_OUT, "step",
                           "IssueBeginFrame");
    last_frame_time_ = args.frame_time;
    client_->OnBeginFrame(copy_args, std::move(frame_timing_details_));
    begin_frame_tracker_.SentBeginFrame(args);
    frame_sink_manager_->DidBeginFrame(frame_sink_id_, args);
    frame_timing_details_.clear();
    UpdateNeedsBeginFramesInternal();
  }

  // Notify surface animation manager if it needs a begin frame.
  if (surface_animation_manager_.NeedsBeginFrame()) {
    surface_animation_manager_.NotifyFrameAdvanced(args.frame_time);

    // Interpolate the frame here, since it is a reliable spot during the
    // animation.
    if (surface_animation_manager_.NeedsBeginFrame()) {
      if (last_activated_surface_id_.is_valid()) {
        auto* surface =
            surface_manager_->GetSurfaceForId(last_activated_surface_id_);
        surface_animation_manager_.InterpolateFrame(surface);
      }
    } else {
      // If notifying causes us to stop needing frames, then update needs begin
      // frames, in case we no longer are interested in receiving begin frames.
      UpdateNeedsBeginFramesInternal();
    }
  }
}

const BeginFrameArgs& CompositorFrameSinkSupport::LastUsedBeginFrameArgs()
    const {
  return last_begin_frame_args_;
}

void CompositorFrameSinkSupport::OnBeginFrameSourcePausedChanged(bool paused) {
  if (client_)
    client_->OnBeginFramePausedChanged(paused);
}

void CompositorFrameSinkSupport::UpdateNeedsBeginFramesInternal() {
  if (!begin_frame_source_)
    return;

  // We require a begin frame if there's a callback pending, or if the client
  // requested it, or if the client needs to get some frame timing details.
  bool needs_begin_frame =
      client_needs_begin_frame_ || !frame_timing_details_.empty() ||
      !pending_surfaces_.empty() ||
      (compositor_frame_callback_ && !callback_received_begin_frame_) ||
      surface_animation_manager_.NeedsBeginFrame();

  if (needs_begin_frame == added_frame_observer_)
    return;

  added_frame_observer_ = needs_begin_frame;
  if (needs_begin_frame) {
    begin_frame_source_->AddObserver(this);
    animation_power_mode_voter_->VoteFor(
        power_scheduler::PowerMode::kAnimation);
  } else {
    begin_frame_source_->RemoveObserver(this);
    animation_power_mode_voter_->ResetVoteAfterTimeout(
        power_scheduler::PowerModeVoter::kAnimationTimeout);
  }
}

void CompositorFrameSinkSupport::AttachCaptureClient(
    CapturableFrameSink::Client* client) {
  DCHECK(!base::Contains(capture_clients_, client));
  capture_clients_.push_back(client);
}

void CompositorFrameSinkSupport::DetachCaptureClient(
    CapturableFrameSink::Client* client) {
  const auto it =
      std::find(capture_clients_.begin(), capture_clients_.end(), client);
  if (it != capture_clients_.end())
    capture_clients_.erase(it);
}

gfx::Size CompositorFrameSinkSupport::GetActiveFrameSize() {
  if (last_activated_surface_id_.is_valid()) {
    Surface* current_surface =
        surface_manager_->GetSurfaceForId(last_activated_surface_id_);
    DCHECK(current_surface);
    if (current_surface->HasActiveFrame()) {
      DCHECK(current_surface->GetActiveFrame().size_in_pixels() ==
             current_surface->size_in_pixels());
      return current_surface->size_in_pixels();
    }
  }
  return gfx::Size();
}

void CompositorFrameSinkSupport::RequestCopyOfOutput(
    PendingCopyOutputRequest pending_copy_output_request) {
  copy_output_requests_.push_back(std::move(pending_copy_output_request));
  if (last_activated_surface_id_.is_valid()) {
    BeginFrameAck ack;
    ack.has_damage = true;
    surface_manager_->SurfaceModified(last_activated_surface_id_, ack);
  }
}

const CompositorFrameMetadata*
CompositorFrameSinkSupport::GetLastActivatedFrameMetadata() {
  if (!last_activated_surface_id_.is_valid())
    return nullptr;
  Surface* surface =
      surface_manager_->GetSurfaceForId(last_activated_surface_id_);
  DCHECK(surface);
  return &surface->GetActiveFrameMetadata();
}

HitTestAggregator* CompositorFrameSinkSupport::GetHitTestAggregator() {
  DCHECK(is_root_);
  return hit_test_aggregator_.get();
}

Surface* CompositorFrameSinkSupport::GetLastCreatedSurfaceForTesting() {
  return surface_manager_->GetSurfaceForId(last_created_surface_id_);
}

// static
const char* CompositorFrameSinkSupport::GetSubmitResultAsString(
    SubmitResult result) {
  switch (result) {
    case SubmitResult::ACCEPTED:
      return "Accepted";
    case SubmitResult::COPY_OUTPUT_REQUESTS_NOT_ALLOWED:
      return "CopyOutputRequests not allowed";
    case SubmitResult::SIZE_MISMATCH:
      return "CompositorFrame size doesn't match surface size";
    case SubmitResult::SURFACE_ID_DECREASED:
      return "LocalSurfaceId sequence numbers decreased";
    case SubmitResult::SURFACE_OWNED_BY_ANOTHER_CLIENT:
      return "Surface belongs to another client";
  }
  NOTREACHED();
  return nullptr;
}

int64_t CompositorFrameSinkSupport::ComputeTraceId() {
  // This is losing some info, but should normally be sufficient to avoid
  // collisions.
  ++trace_sequence_;
  uint64_t client = (frame_sink_id_.client_id() & 0xffff);
  uint64_t sink = (frame_sink_id_.sink_id() & 0xffff);
  return (client << 48) | (sink << 32) | trace_sequence_;
}

bool CompositorFrameSinkSupport::ShouldSendBeginFrame(
    base::TimeTicks frame_time) {
  // We should throttle OnBeginFrame() if it has been less than
  // |begin_frame_interval_| since the last one was sent because clients have
  // requested to update at such rate.
  const bool should_throttle_as_requested =
      begin_frame_interval_ > base::TimeDelta() &&
      (frame_time - last_frame_time_) < begin_frame_interval_;
  // We might throttle this OnBeginFrame() if it's been less than a second since
  // the last one was sent, either because clients are unresponsive or have
  // submitted too many undrawn frames.
  const bool can_throttle_if_unresponsive_or_excessive =
      frame_time - last_frame_time_ < base::TimeDelta::FromSeconds(1);

  // If there are pending timing details from the previous frame(s),
  // then the client needs to receive the begin-frame.
  if (!frame_timing_details_.empty() && !should_throttle_as_requested) {
    RecordShouldSendBeginFrame("SendFrameTiming");
    return true;
  }

  if (!client_needs_begin_frame_) {
    RecordShouldSendBeginFrame("StopNotRequested");
    return false;
  }

  // Stop sending BeginFrames to clients that are totally unresponsive.
  if (begin_frame_tracker_.ShouldStopBeginFrame()) {
    RecordShouldSendBeginFrame("StopUnresponsiveClient");
    return false;
  }

  // Throttle clients that are unresponsive.
  if (can_throttle_if_unresponsive_or_excessive &&
      begin_frame_tracker_.ShouldThrottleBeginFrame()) {
    RecordShouldSendBeginFrame("ThrottleUnresponsiveClient");
    return false;
  }

  if (!last_activated_surface_id_.is_valid()) {
    RecordShouldSendBeginFrame("SendNoActiveSurface");
    return true;
  }

  // We should never throttle BeginFrames if there is another client waiting for
  // this client to submit a frame.
  if (surface_manager_->HasBlockedEmbedder(frame_sink_id_)) {
    RecordShouldSendBeginFrame("SendBlockedEmbedded");
    return true;
  }

  if (should_throttle_as_requested) {
    RecordShouldSendBeginFrame("ThrottleRequested");
    return false;
  }

  Surface* surface =
      surface_manager_->GetSurfaceForId(last_activated_surface_id_);

  DCHECK(surface);
  DCHECK(surface->HasActiveFrame());

  uint64_t active_frame_index = surface->GetActiveFrameIndex();

  // Since we have an active frame, and frame indexes strictly increase
  // during the lifetime of the CompositorFrameSinkSupport, our active frame
  // index must be at least as large as our last drawn frame index.
  DCHECK_GE(active_frame_index, last_drawn_frame_index_);

  // Throttle clients that have submitted too many undrawn frames.
  uint64_t num_undrawn_frames = active_frame_index - last_drawn_frame_index_;
  if (can_throttle_if_unresponsive_or_excessive &&
      num_undrawn_frames > kUndrawnFrameLimit) {
    RecordShouldSendBeginFrame("ThrottleUndrawnFrames");
    return false;
  }

  // No other conditions apply so send the begin frame.
  RecordShouldSendBeginFrame("SendDefault");
  return true;
}

bool CompositorFrameSinkSupport::IsEvicted(
    const LocalSurfaceId& local_surface_id) const {
  return local_surface_id.embed_token() ==
             last_evicted_local_surface_id_.embed_token() &&
         local_surface_id.parent_sequence_number() <=
             last_evicted_local_surface_id_.parent_sequence_number();
}

void CompositorFrameSinkSupport::CheckPendingSurfaces() {
  if (pending_surfaces_.empty())
    return;
  base::flat_set<Surface*> pending_surfaces(pending_surfaces_);
  for (Surface* surface : pending_surfaces) {
    surface->ActivateIfDeadlinePassed();
  }
}

}  // namespace viz
