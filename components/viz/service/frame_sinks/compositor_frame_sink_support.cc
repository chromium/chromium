// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "components/power_scheduler/power_mode.h"
#include "components/power_scheduler/power_mode_arbiter.h"
#include "components/power_scheduler/power_mode_voter.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_counter.h"
#include "components/viz/service/frame_sinks/frame_sink_bundle_impl.h"
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
      // Don't track the root surface for PowerMode voting. All child surfaces
      // are tracked individually instead, and tracking the root surface could
      // override votes from the children.
      power_mode_voter_(
          is_root ? nullptr
                  : power_scheduler::PowerModeArbiter::GetInstance()->NewVoter(
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

  if (bundle_id_.has_value()) {
    if (auto* bundle = frame_sink_manager_->GetFrameSinkBundle(*bundle_id_)) {
      bundle->RemoveFrameSink(this);
    }
  }
}

FrameTimingDetailsMap CompositorFrameSinkSupport::TakeFrameTimingDetailsMap() {
  FrameTimingDetailsMap map;
  map.swap(frame_timing_details_);

  // As we're clearing `frame_timing_details_`, we might no longer need
  // BeginFrame if delivering presentation feedback was the only reason.
  UpdateNeedsBeginFramesInternal();
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
    // When detaching from source, CompositorFrameSinkClient needs to know that
    // there are no more OnBeginFrame. Otherwise, client in renderer could wait
    // OnBeginFrame forever. e.g., crbug.com/1335000.
    // OnBeginFrameSourcePausedChanged(false) is not handled here because it's
    // handled in AddObserver() depending on current status of begin frame
    // source.
    if (!begin_frame_source)
      OnBeginFrameSourcePausedChanged(true);
    added_frame_observer_ = false;
  }

  auto* old_source = begin_frame_source_.get();
  begin_frame_source_ = begin_frame_source;

  FrameSinkBundleImpl* bundle = nullptr;
  if (bundle_id_) {
    bundle = frame_sink_manager_->GetFrameSinkBundle(*bundle_id_);
    if (!bundle) {
      // Our bundle has been destroyed.
      ScheduleSelfDestruction();
      return;
    }
    bundle->UpdateFrameSink(this, old_source);
  }

  UpdateNeedsBeginFramesInternal();
}

void CompositorFrameSinkSupport::SetBundle(const FrameSinkBundleId& bundle_id) {
  auto* bundle = frame_sink_manager_->GetFrameSinkBundle(bundle_id);
  if (!bundle) {
    // The bundle is gone already, so force the client to re-establish their
    // CompositorFrameSink.
    ScheduleSelfDestruction();
    return;
  }

  bundle->AddFrameSink(this);
  bundle_id_ = bundle_id;
  UpdateNeedsBeginFramesInternal();
}

void CompositorFrameSinkSupport::ThrottleBeginFrame(base::TimeDelta interval) {
  begin_frame_interval_ = interval;
}

void CompositorFrameSinkSupport::OnSurfaceCommitted(Surface* surface) {
  if (surface->HasPendingFrame()) {
    // Make sure we periodically check if the frame should activate.
    pending_surfaces_.insert(surface);
    UpdateNeedsBeginFramesInternal();
  }
}

void CompositorFrameSinkSupport::OnSurfaceActivated(Surface* surface) {
  DCHECK(surface);
  DCHECK(surface->HasActiveFrame());
  DCHECK(!surface->HasPendingFrame());

  pending_surfaces_.erase(surface);
  if (pending_surfaces_.empty())
    UpdateNeedsBeginFramesInternal();

  for (const auto& directive :
       surface->GetActiveFrameMetadata().transition_directives) {
    ProcessCompositorFrameTransitionDirective(directive, surface);
  }

  // The directives above generate TransferableResources which are required to
  // replace shared elements with the corresponding cached snapshots. This step
  // must be done after processing directives above.
  if (surface_animation_manager_)
    surface_animation_manager_->ReplaceSharedElementResources(surface);

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

void CompositorFrameSinkSupport::SendCompositorFrameAck() {
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

  current_capture_bounds_ = frame.metadata.capture_bounds;
  for (CapturableFrameSink::Client* client : capture_clients_) {
    client->OnFrameDamaged(frame_size_in_pixels, damage_rect,
                           expected_display_time, frame.metadata);
  }
}

bool CompositorFrameSinkSupport::IsVideoCaptureStarted() {
  return number_clients_capturing_ > 0;
}

base::flat_set<base::PlatformThreadId>
CompositorFrameSinkSupport::GetThreadIds() {
  return thread_ids_;
}

void CompositorFrameSinkSupport::OnSurfaceDestroyed(Surface* surface) {
  pending_surfaces_.erase(surface);

  if (surface->surface_id() == last_activated_surface_id_)
    last_activated_surface_id_ = SurfaceId();

  if (surface->surface_id() == last_created_surface_id_)
    last_created_surface_id_ = SurfaceId();

  if (!features::IsOnBeginFrameAcksEnabled() || !client_ ||
      surface_returned_resources_.empty()) {
    return;
  }
  client_->ReclaimResources(std::move(surface_returned_resources_));
  surface_returned_resources_.clear();
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
  if (surface_animation_manager_)
    surface_animation_manager_->RefResources(resources);
  surface_resource_holder_.RefResources(resources);
}

void CompositorFrameSinkSupport::UnrefResources(
    std::vector<ReturnedResource> resources) {
  // |surface_animation_manager_| allocates ResourceIds in a different range
  // than the client so it can process returned resources before
  // |surface_resource_holder_|.
  if (surface_animation_manager_)
    surface_animation_manager_->UnrefResources(resources);
  surface_resource_holder_.UnrefResources(std::move(resources));
}

void CompositorFrameSinkSupport::ReturnResources(
    std::vector<ReturnedResource> resources) {
  if (resources.empty())
    return;

  // When features::OnBeginFrameAcks is disabled we attempt to return resources
  // in DidReceiveCompositorFrameAck. However if there is no
  // `ack_pending_count_` then we don't expect that signal soon. In which case
  // we return the resources to the `client_` now.
  //
  // When features::OnBeginFrameAcks is enabled we attempt to return resources
  // during the next OnBeginFrame. However if we currently do not
  // `needs_begin_frame_` or if we have been disconnected from a
  // `begin_frame_source_` then we don't expect that signal soon. In which case
  // we return the resources to the `client_` now.
  if (!ack_pending_count_ && client_ &&
      (!features::IsOnBeginFrameAcksEnabled() ||
       (!needs_begin_frame_ || !begin_frame_source_))) {
    client_->ReclaimResources(std::move(resources));
    return;
  }

  std::move(resources.begin(), resources.end(),
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

  if (frame_sink_manager_->frame_counter()) {
    frame_sink_manager_->frame_counter()->SetFrameSinkType(frame_sink_id_,
                                                           frame_sink_type_);
  }
}

void CompositorFrameSinkSupport::SetThreadIds(
    bool from_untrusted_client,
    base::flat_set<base::PlatformThreadId> unverified_thread_ids) {
  if (!from_untrusted_client ||
      frame_sink_manager_->VerifySandboxedThreadIds(unverified_thread_ids)) {
    thread_ids_ = unverified_thread_ids;
  }
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
    absl::optional<HitTestRegionList> hit_test_region_list,
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
    absl::optional<HitTestRegionList> hit_test_region_list,
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

  if (frame.metadata.begin_frame_ack.frame_id.source_id ==
      BeginFrameArgs::kManualSourceId) {
    pending_manual_begin_frame_source_ = true;
  }

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
      // Pending frames are processed in OnSurfaceCommitted.
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
  bool was_pending_manual_begin_frame_source_ =
      pending_manual_begin_frame_source_;
  ack_pending_count_--;
  if (!ack_pending_count_) {
    pending_manual_begin_frame_source_ = false;
  }
  if (!client_)
    return;

  // If we have a callback, we only return the resource on onBeginFrame.
  if (compositor_frame_callback_) {
    callback_received_receive_ack_ = true;
    UpdateNeedsBeginFramesInternal();
    HandleCallback();
    return;
  }

  if (features::IsOnBeginFrameAcksEnabled() &&
      !was_pending_manual_begin_frame_source_) {
    return;
  }

  client_->DidReceiveCompositorFrameAck(std::move(surface_returned_resources_));
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
  // Override with the throttled interval if one has been set. Otherwise,
  // consumers will assume that the default vsync interval was the target and
  // that the frames are presented too late when in fact, this is intentional.
  if (begin_frame_interval_.is_positive() &&
      details.presentation_feedback.interval.is_positive() &&
      features::ShouldOverrideThrottledFrameRateParams()) {
    details.presentation_feedback.interval = begin_frame_interval_;
  }
  pending_received_frame_times_.erase(received_frame_timestamp);

  // We should only ever get one PresentationFeedback per frame_token.
  DCHECK(frame_timing_details_.find(frame_token) ==
         frame_timing_details_.end());
  frame_timing_details_.emplace(frame_token, details);

  if (!feedback.failed() && frame_sink_manager_->frame_counter()) {
    frame_sink_manager_->frame_counter()->AddPresentedFrame(frame_sink_id_,
                                                            feedback.timestamp);
  }

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
  ReturnResources(std::move(resources));
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

  BeginFrameArgs adjusted_args = args;
  if (begin_frame_interval_.is_positive() &&
      features::ShouldOverrideThrottledFrameRateParams()) {
    adjusted_args.interval = begin_frame_interval_;
    // Deadline is not necessarily frame_time + interval. For example, it may
    // incorporate an estimate for the frame's draw/swap time, so it's
    // desirable to preserve any offset from the next scheduled frame.
    base::TimeDelta offset_from_next_scheduled_frame =
        args.deadline - (args.frame_time + args.interval);
    adjusted_args.deadline = args.frame_time + begin_frame_interval_ +
                             offset_from_next_scheduled_frame;
  }

  bool send_begin_frame_to_client =
      client_ && ShouldSendBeginFrame(adjusted_args.frame_time);
  if (send_begin_frame_to_client) {
    if (last_activated_surface_id_.is_valid())
      surface_manager_->SurfaceDamageExpected(last_activated_surface_id_,
                                              adjusted_args);
    last_begin_frame_args_ = adjusted_args;

    // Force full frame if surface not yet activated to ensure surface creation.
    if (!last_activated_surface_id_.is_valid())
      adjusted_args.animate_only = false;

    adjusted_args.trace_id = ComputeTraceId();
    TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                           TRACE_ID_GLOBAL(adjusted_args.trace_id),
                           TRACE_EVENT_FLAG_FLOW_OUT, "step",
                           "IssueBeginFrame");
    adjusted_args.frames_throttled_since_last = frames_throttled_since_last_;
    frames_throttled_since_last_ = 0;

    last_frame_time_ = adjusted_args.frame_time;
    if (features::IsOnBeginFrameAcksEnabled()) {
      client_->OnBeginFrame(adjusted_args, std::move(frame_timing_details_),
                            !ack_pending_count_,
                            std::move(surface_returned_resources_));
      surface_returned_resources_.clear();
    } else {
      client_->OnBeginFrame(adjusted_args, std::move(frame_timing_details_),
                            /*frame_ack=*/false,
                            std::vector<ReturnedResource>());
    }
    begin_frame_tracker_.SentBeginFrame(adjusted_args);
    frame_sink_manager_->DidBeginFrame(frame_sink_id_, adjusted_args);
    frame_timing_details_.clear();
    UpdateNeedsBeginFramesInternal();
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
  // requested it, or if the client needs to get some frame timing details, or
  // if there are resources to return.
  needs_begin_frame_ =
      (client_needs_begin_frame_ || !frame_timing_details_.empty() ||
       !pending_surfaces_.empty() ||
       (compositor_frame_callback_ && !callback_received_begin_frame_) ||
       (features::IsOnBeginFrameAcksEnabled() &&
        !surface_returned_resources_.empty()));

  if (bundle_id_.has_value()) {
    // When bundled with other sinks, observation of BeginFrame notifications is
    // always delegated to the bundle.
    if (added_frame_observer_) {
      StopObservingBeginFrameSource();
    }
    if (auto* bundle = frame_sink_manager_->GetFrameSinkBundle(*bundle_id_)) {
      bundle->SetSinkNeedsBeginFrame(frame_sink_id_.sink_id(),
                                     needs_begin_frame_);
    }
    return;
  }

  if (needs_begin_frame_ == added_frame_observer_)
    return;

  if (needs_begin_frame_) {
    StartObservingBeginFrameSource();
  } else {
    StopObservingBeginFrameSource();
  }
}

void CompositorFrameSinkSupport::StartObservingBeginFrameSource() {
  added_frame_observer_ = true;
  begin_frame_source_->AddObserver(this);
  if (power_mode_voter_) {
    power_mode_voter_->VoteFor(
        frame_sink_type_ == mojom::CompositorFrameSinkType::kMediaStream ||
                frame_sink_type_ == mojom::CompositorFrameSinkType::kVideo
            ? power_scheduler::PowerMode::kVideoPlayback
            : power_scheduler::PowerMode::kAnimation);
  }
}

void CompositorFrameSinkSupport::StopObservingBeginFrameSource() {
  added_frame_observer_ = false;
  begin_frame_source_->RemoveObserver(this);
  if (power_mode_voter_) {
    power_mode_voter_->ResetVoteAfterTimeout(
        frame_sink_type_ == mojom::CompositorFrameSinkType::kMediaStream ||
                frame_sink_type_ == mojom::CompositorFrameSinkType::kVideo
            ? power_scheduler::PowerModeVoter::kVideoTimeout
            : power_scheduler::PowerModeVoter::kAnimationTimeout);
  }
}

const FrameSinkId& CompositorFrameSinkSupport::GetFrameSinkId() const {
  return frame_sink_id_;
}

void CompositorFrameSinkSupport::AttachCaptureClient(
    CapturableFrameSink::Client* client) {
  DCHECK(!base::Contains(capture_clients_, client));
  capture_clients_.push_back(client);
  if (client->IsVideoCaptureStarted())
    OnClientCaptureStarted();
}

void CompositorFrameSinkSupport::DetachCaptureClient(
    CapturableFrameSink::Client* client) {
  const auto it = base::ranges::find(capture_clients_, client);
  if (it != capture_clients_.end())
    capture_clients_.erase(it);
  if (client->IsVideoCaptureStarted())
    OnClientCaptureStopped();
}

void CompositorFrameSinkSupport::OnClientCaptureStarted() {
  if (number_clients_capturing_++ == 0) {
    // First client started capturing.
    frame_sink_manager_->OnCaptureStarted(frame_sink_id_);
  }
}

void CompositorFrameSinkSupport::OnClientCaptureStopped() {
  DCHECK_GT(number_clients_capturing_, 0u);
  if (--number_clients_capturing_ == 0) {
    // The last client has stopped capturing.
    frame_sink_manager_->OnCaptureStopped(frame_sink_id_);
  }
}

gfx::Rect CompositorFrameSinkSupport::GetCopyOutputRequestRegion(
    const VideoCaptureSubTarget& sub_target) const {
  // We will either have a subtree ID or a region capture crop_id, but not both.
  if (absl::holds_alternative<RegionCaptureCropId>(sub_target)) {
    const auto it = current_capture_bounds_.bounds().find(
        absl::get<RegionCaptureCropId>(sub_target));
    if (it != current_capture_bounds_.bounds().end()) {
      return it->second;
    }
    return {};
  }

  if (!last_activated_surface_id_.is_valid())
    return {};

  Surface* current_surface =
      surface_manager_->GetSurfaceForId(last_activated_surface_id_);
  DCHECK(current_surface);
  if (!current_surface->HasActiveFrame()) {
    return {};
  }

  // We can exit early if there is no subtree, otherwise we need to
  // intersect the bounds.
  const CompositorFrame& frame = current_surface->GetActiveFrame();
  if (!absl::holds_alternative<SubtreeCaptureId>(sub_target)) {
    return gfx::Rect(frame.size_in_pixels());
  }

  // Now we know we don't have a crop_id and we do have a subtree ID.
  for (const auto& render_pass : frame.render_pass_list) {
    if (render_pass->subtree_capture_id ==
        absl::get<SubtreeCaptureId>(sub_target)) {
      return render_pass->subtree_size.IsEmpty()
                 ? render_pass->output_rect
                 : gfx::Rect(render_pass->subtree_size);
    }
  }

  // No target exists and no CopyOutputRequest will be added.
  // If we reach here, it means we only want to capture a subtree but
  // were unable to find it in a render pass--so don't capture anything.
  return {};
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
      ShouldThrottleBeginFrameAsRequested(frame_time);
  // We might throttle this OnBeginFrame() if it's been less than a second
  // since the last one was sent, either because clients are unresponsive or
  // have submitted too many undrawn frames.
  const bool can_throttle_if_unresponsive_or_excessive =
      frame_time - last_frame_time_ < base::Seconds(1);

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

  // We should never throttle BeginFrames if there is another client waiting
  // for this client to submit a frame.
  if (surface_manager_->HasBlockedEmbedder(frame_sink_id_)) {
    RecordShouldSendBeginFrame("SendBlockedEmbedded");
    return true;
  }

  if (should_throttle_as_requested) {
    ++frames_throttled_since_last_;
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

  // Throttle clients that have submitted too many undrawn frames, unless the
  // active frame requests that it doesn't.
  uint64_t num_undrawn_frames = active_frame_index - last_drawn_frame_index_;
  if (can_throttle_if_unresponsive_or_excessive &&
      num_undrawn_frames > kUndrawnFrameLimit &&
      surface->GetActiveFrameMetadata().may_throttle_if_undrawn_frames) {
    RecordShouldSendBeginFrame("ThrottleUndrawnFrames");
    return false;
  }

  // No other conditions apply so send the begin frame.
  RecordShouldSendBeginFrame("SendDefault");
  return true;
}

void CompositorFrameSinkSupport::CheckPendingSurfaces() {
  if (pending_surfaces_.empty())
    return;
  base::flat_set<Surface*> pending_surfaces(pending_surfaces_);
  for (Surface* surface : pending_surfaces) {
    surface->ActivateIfDeadlinePassed();
  }
}

bool CompositorFrameSinkSupport::ShouldThrottleBeginFrameAsRequested(
    base::TimeTicks frame_time) {
  // It is not good enough to only check whether
  // |time_since_last_frame| < |begin_frame_interval_|. There are 2 factors
  // complicating this (examples assume a 30Hz throttled frame rate):
  // 1) The precision of timing between frames is in microseconds, which
  //    can result in error accumulation over several throttled frames. For
  //    instance, on a 60Hz display, the first frame is produced at 0.016666
  //    seconds, and the second at (0.016666 + 0.016666 = 0.033332) seconds.
  //    base::Hertz(30) is 0.033333 seconds, so the second frame is considered
  //    to have been produced too fast, and is therefore throttled.
  // 2) Small system error in the frame timestamps (often on the order of a few
  //    microseconds). For example, the first frame may be produced at 0.016662
  //    seconds (instead of 0.016666), so the second frame's timestamp is
  //    0.016662 + 0.016666 = 0.033328 and incorrectly gets throttled.
  //
  // To correct for this: Ceil the time since last frame to the nearest 100us.
  // Building off the example above:
  // Frame 1 time -> 0.016662 -> 0.0167 -> Throttle
  // Frame 2 time -> 0.016662 + 0.016666 = 0.033328 -> 0.0334 -> Don't Throttle
  static constexpr base::TimeDelta kFrameTimeQuantization =
      base::Microseconds(100);
  base::TimeDelta time_since_last_frame = frame_time - last_frame_time_;
  return begin_frame_interval_.is_positive() &&
         time_since_last_frame.CeilToMultiple(kFrameTimeQuantization) <
             begin_frame_interval_;
}

void CompositorFrameSinkSupport::ProcessCompositorFrameTransitionDirective(
    const CompositorFrameTransitionDirective& directive,
    Surface* surface) {
  switch (directive.type()) {
    case CompositorFrameTransitionDirective::Type::kSave:
      // Initialize this before creating the SurfaceAnimationManager since the
      // save operation may execute synchronously.
      in_flight_save_sequence_id_ = directive.sequence_id();
      surface_animation_manager_ = SurfaceAnimationManager::CreateWithSave(
          directive, surface, frame_sink_manager_->shared_bitmap_manager(),
          base::BindOnce(&CompositorFrameSinkSupport::
                             OnCompositorFrameTransitionDirectiveProcessed,
                         base::Unretained(this)));
      break;
    case CompositorFrameTransitionDirective::Type::kAnimateRenderer:
      // The save operation must have been executed before we see an animate
      // directive.
      if (in_flight_save_sequence_id_ != 0) {
        LOG(ERROR)
            << "Ignoring animate directive, save operation pending completion";
        break;
      }

      if (directive.navigation_id()) {
        if (surface_animation_manager_) {
          LOG(ERROR) << "Deleting existing SurfaceAnimationManager for "
                        "transition with navigation_id : "
                     << directive.navigation_id();
        }

        surface_animation_manager_ =
            frame_sink_manager_->TakeSurfaceAnimationManager(
                directive.navigation_id());
      }

      if (surface_animation_manager_)
        surface_animation_manager_->Animate();
      else
        LOG(ERROR) << "Animate directive with no saved data.";

      break;
    case CompositorFrameTransitionDirective::Type::kRelease:
      surface_animation_manager_.reset();

      // This `surface_animation_manager_` could correspond to an in-flight
      // save, reset the tracking here.
      in_flight_save_sequence_id_ = 0;

      // If we had a `navigation_id`, also make sure to clean up the
      // `frame_sink_manager_` in case the animation was never started (which
      // would be the case if the destination renderer didn't opt-in to the
      // animation behavior). Note that this operation is harmless if there
      // is no surface animation manager to clear.
      if (directive.navigation_id()) {
        frame_sink_manager_->ClearSurfaceAnimationManager(
            directive.navigation_id());
      }
      break;
  }
}

void CompositorFrameSinkSupport::OnCompositorFrameTransitionDirectiveProcessed(
    const CompositorFrameTransitionDirective& directive) {
  DCHECK_EQ(directive.type(), CompositorFrameTransitionDirective::Type::kSave)
      << "Only save directives need to be ack'd back to the client";

  if (client_) {
    client_->OnCompositorFrameTransitionDirectiveProcessed(
        directive.sequence_id());
  }

  // There could be an ID mismatch if there are consecutive save operations
  // before the first one is ack'd. This should never happen but handled safely
  // here since the directives are untrusted input from the renderer.
  if (in_flight_save_sequence_id_ == directive.sequence_id() &&
      directive.navigation_id()) {
    // Note that this can fail if there is already a cached
    // SurfaceAnimationManager for this |navigation_id|. Should never happen
    // but handled safely because its untrusted input from the renderer.
    frame_sink_manager_->CacheSurfaceAnimationManager(
        directive.navigation_id(), std::move(surface_animation_manager_));
  }

  in_flight_save_sequence_id_ = 0;
}

bool CompositorFrameSinkSupport::IsEvicted(
    const LocalSurfaceId& local_surface_id) const {
  return local_surface_id.embed_token() ==
             last_evicted_local_surface_id_.embed_token() &&
         local_surface_id.parent_sequence_number() <=
             last_evicted_local_surface_id_.parent_sequence_number();
}

SurfaceAnimationManager*
CompositorFrameSinkSupport::GetSurfaceAnimationManagerForTesting() {
  return surface_animation_manager_.get();
}

void CompositorFrameSinkSupport::DestroySelf() {
  // SUBTLE: We explicitly copy `frame_sink_id_` because
  // DestroyCompositorFrameSink takes the FrameSinkId by reference and may
  // dereference it after destroying `this`.
  FrameSinkId frame_sink_id = frame_sink_id_;
  frame_sink_manager_->DestroyCompositorFrameSink(frame_sink_id,
                                                  base::DoNothing());
}

void CompositorFrameSinkSupport::ScheduleSelfDestruction() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&CompositorFrameSinkSupport::DestroySelf,
                                weak_factory_.GetWeakPtr()));
}

}  // namespace viz
