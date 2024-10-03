// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "components/input/utils.h"
#include "components/viz/common/constants.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_counter.h"
#include "components/viz/service/frame_sinks/frame_sink_bundle_impl.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/layers/layer_context_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_reference.h"
#include "components/viz/service/transitions/surface_animation_manager.h"
#include "media/filters/video_cadence_estimator.h"
#include "mojo/public/cpp/system/platform_handle.h"

// This determines whether the provided time since last interval corresponds
// to a cadence frame that needs to be rendered.
bool HasElapsedCadenceInterval(
    base::TimeDelta render_interval,
    base::TimeDelta frame_duration,
    base::TimeDelta time_since_last_render_interval) {
  base::TimeDelta time_at_next_interval =
      time_since_last_render_interval + render_interval;
  base::TimeDelta tolerance = render_interval * 0.5;
  return time_at_next_interval > (frame_duration + tolerance);
}

namespace viz {
namespace {

bool RecordShouldSendBeginFrame(const std::string& reason, bool should_send) {
  TRACE_EVENT2("viz", "SendBeginFrameDecision", "reason", reason, "should_send",
               should_send);
  return should_send;
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

// Takes a given `rect` and then transforms it to a given space via
// `transform_to_space` to be interested by an `intersectee_in_space`, then
// transforms it back to its original space. NOTE: `transform_to_space` must be
// invertible.
gfx::Rect IntersectInSpace(const gfx::Rect& rect,
                           const gfx::Transform& transform_to_space,
                           const gfx::Rect& intersectee_in_space) {
  const gfx::Rect rect_in_space = transform_to_space.MapRect(rect);

  const gfx::Rect intersected_in_space =
      gfx::IntersectRects(rect_in_space, intersectee_in_space);

  const std::optional<gfx::Rect> intersected =
      transform_to_space.InverseMapRect(intersected_in_space);

  return intersected.value_or(gfx::Rect{});
}

void RemoveSurfaceReferenceAndDispatchCopyOutputRequestCallback(
    base::WeakPtr<FrameSinkManagerImpl> frame_sink_manager,
    const SurfaceId& holds_ref_surface_id,
    const blink::SameDocNavigationScreenshotDestinationToken& destination_token,
    std::unique_ptr<CopyOutputResult> result) {
  if (!frame_sink_manager) {
    return;
  }
  if (auto* surface_holds_ref =
          frame_sink_manager->surface_manager()->GetSurfaceForId(
              holds_ref_surface_id)) {
    surface_holds_ref->ResetPendingCopySurfaceId();
  }
  if (frame_sink_manager) {
    // Send the IPC to the browser process even if `result` is empty. The empty
    // result will be handled on the browser side.
    frame_sink_manager->OnScreenshotCaptured(destination_token,
                                             std::move(result));
  } else {
    // TODO(https://crbug.com/369936885): Remove this once the crash reason is
    // confirmed.
    base::debug::DumpWithoutCrashing();
  }
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
      use_blit_request_for_view_transition_(base::FeatureList::IsEnabled(
          features::kBlitRequestsForViewTransition)) {
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
  // When we unregister `this` from `frame_sink_manager_` from above,
  // `FrameSinkManagerImpl::DiscardPendingCopyOfOutputRequests` clears out all
  // the outstanding requests.
  // However there are other destruction code paths that don't exercise
  // `DiscardPendingCopyOfOutputRequests` (e.g, in unittests we might not have a
  // `RootCompositorFrameSinkImpl`, such that no `ExternalBeginFrameSourceMojo`
  // is added as an observer of `frame_sink_manager_`). Therefore we explicitly
  // clear the requests here regardless.
  ClearAllPendingCopyOutputRequests();

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

void CompositorFrameSinkSupport::SetLastKnownVsync(
    base::TimeDelta vsync_interval) {
  if (last_known_vsync_interval_ == vsync_interval) {
    return;
  }
  last_known_vsync_interval_ = vsync_interval;
  ThrottleBeginFrame(begin_frame_interval_, /*simple_cadence_only=*/true);
}

bool CompositorFrameSinkSupport::ThrottleBeginFrame(base::TimeDelta interval,
                                                    bool simple_cadence_only) {
  if (!interval.is_positive()) {
    // Remove any existing throttling
    begin_frame_interval_ = base::TimeDelta();
    return true;
  }

  if (!last_known_vsync_interval_.is_positive() || !simple_cadence_only) {
    // No known vsync interval or forcing throttle interval.
    begin_frame_interval_ = interval;
    return true;
  }

  if (media::VideoCadenceEstimator::HasSimpleCadence(
          last_known_vsync_interval_, interval, kMaxTimeUntilNextGlitch)) {
    begin_frame_interval_ = interval;
    return true;
  } else {
    begin_frame_interval_ = base::TimeDelta();
    return false;
  }
}

void CompositorFrameSinkSupport::ApplyPreferredFrameRate(uint64_t source_id) {
  // Skip throttling for very small changes in frame interval.
  // A value of 2 ms proved to be enough to not have throttle firing during
  // a constant video playback but can be changed to a higher value if
  // over firing occurs in some edge case while always aiming to keep it
  // lower than a full frame interval.
  if ((last_known_frame_interval_ - preferred_frame_interval_).magnitude() >
      base::Milliseconds(2)) {
    TRACE_EVENT_INSTANT2("viz", "Set sink framerate", TRACE_EVENT_SCOPE_THREAD,
                         "interval", preferred_frame_interval_, "sourceid",
                         source_id);
    last_known_frame_interval_ = preferred_frame_interval_;
    // Only throttle simple cadences.
    ThrottleBeginFrame(preferred_frame_interval_,
                       /*simple_cadence_only=*/true);
  }
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
  SurfaceAnimationManager::ReplaceSharedElementResources(
      surface, view_transition_token_to_animation_manager_);

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

  if (!ShouldMergeBeginFrameWithAcks() || !client_ ||
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
  // If the frame was submitted locally (from inside viz), do not tell the
  // client about it, since the client did not send it.
  if (frame_token != kLocalFrameToken) {
    DidPresentCompositorFrame(frame_token, draw_start_timestamp, swap_timings,
                              feedback);
  }
}

void CompositorFrameSinkSupport::RefResources(
    const std::vector<TransferableResource>& resources) {
  ForAllReservedResourceDelegates(
      [&resources](ReservedResourceDelegate& delegate) {
        delegate.RefResources(resources);
      });

  surface_resource_holder_.RefResources(resources);
}

void CompositorFrameSinkSupport::UnrefResources(
    std::vector<ReturnedResource> resources) {
  // `ReservedResourceDelegate` allocates ResourceIds in a different range
  // than the client so it can process returned resources before
  // |surface_resource_holder_|.
  ForAllReservedResourceDelegates(
      [&resources](ReservedResourceDelegate& delegate) {
        delegate.UnrefResources(resources);
      });

  surface_resource_holder_.UnrefResources(std::move(resources));
}

void CompositorFrameSinkSupport::ReturnResources(
    std::vector<ReturnedResource> resources) {
  if (resources.empty())
    return;

  if (layer_context_) {
    // Resource management is delegated to LayerContext when it's in use.
    layer_context_->ReturnResources(std::move(resources));
    return;
  }

  // When features::OnBeginFrameAcks is disabled we attempt to return resources
  // in DidReceiveCompositorFrameAck. However if there are no pending frames
  // then we don't expect that signal soon. In which case we return the
  // resources to the `client_` now.
  //
  // When features::OnBeginFrameAcks is enabled we attempt to return resources
  // during the next OnBeginFrame. However if we currently do not
  // `needs_begin_frame_` or if we have been disconnected from a
  // `begin_frame_source_` then we don't expect that signal soon. In which case
  // we return the resources to the `client_` now.
  if (pending_frames_.empty() && client_ &&
      (!ShouldMergeBeginFrameWithAcks() ||
       (!needs_begin_frame_ || !begin_frame_source_))) {
    client_->ReclaimResources(std::move(resources));
    return;
  }

  std::move(resources.begin(), resources.end(),
            std::back_inserter(surface_returned_resources_));
}

void CompositorFrameSinkSupport::ReceiveFromChild(
    const std::vector<TransferableResource>& resources) {
  ForAllReservedResourceDelegates(
      [&resources](ReservedResourceDelegate& delegate) {
        delegate.ReceiveFromChild(resources);
      });

  surface_resource_holder_.ReceiveFromChild(resources);
}

std::vector<PendingCopyOutputRequest>
CompositorFrameSinkSupport::TakeCopyOutputRequests(
    const LocalSurfaceId& latest_local_id) {
  std::vector<PendingCopyOutputRequest> results;
  for (auto it = copy_output_requests_.begin();
       it != copy_output_requests_.end();) {
    // Pick up the requests that require an exact `LocalSurfaceId` match.
    if (it->capture_exact_surface_id) {
      // `ui::DelegatedFrameHostAndroid` won't send a `CopyOutputRequest`
      // without a valid `LocalSurfaceId`. This is guaranteed as we can't
      // serialize/deserialize an empty `LocalSurfaceId`.
      CHECK(it->local_surface_id.is_valid());
      if (it->local_surface_id == latest_local_id) {
        results.push_back(std::move(*it));
        it = copy_output_requests_.erase(it);
      } else {
        ++it;
      }
    }
    // Requests with a non-valid local id should be satisfied as soon as
    // possible.
    else if (!it->local_surface_id.is_valid() ||  // NOLINT
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
  if (client_) {
    client_->OnSurfaceEvicted(id);
  }
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

void CompositorFrameSinkSupport::SetWantsBeginFrameAcks() {
  wants_begin_frame_acks_ = true;
}

void CompositorFrameSinkSupport::SetAutoNeedsBeginFrame() {
  auto_needs_begin_frame_ = true;
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

void CompositorFrameSinkSupport::BindLayerContext(
    mojom::PendingLayerContext& context) {
  layer_context_ = std::make_unique<LayerContextImpl>(this, context);
}

void CompositorFrameSinkSupport::SetThreadIds(
    bool from_untrusted_client,
    base::flat_set<base::PlatformThreadId> unverified_thread_ids) {
  if (!from_untrusted_client) {
    thread_ids_ = std::move(unverified_thread_ids);
    return;
  }
  frame_sink_manager_->VerifySandboxedThreadIds(
      unverified_thread_ids,
      base::BindOnce(
          &CompositorFrameSinkSupport::UpdateThreadIdsPostVerification,
          weak_factory_.GetWeakPtr(), unverified_thread_ids));
}

void CompositorFrameSinkSupport::UpdateThreadIdsPostVerification(
    base::flat_set<base::PlatformThreadId> thread_ids,
    bool passed_verification) {
  if (passed_verification) {
    thread_ids_ = std::move(thread_ids);
  } else {
    TRACE_EVENT_INSTANT("viz,android.adpf",
                        "FailedToUpdateThreadIdsPostVerification", "thread_ids",
                        thread_ids);
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
  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(ack.trace_id), [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_DID_NOT_PRODUCE_FRAME);
        frame_sink_id_.WriteIntoTrace(ctx.Wrap(data->set_frame_sink_id()));
        data->set_display_trace_id(ack.trace_id);
      });
  DCHECK(ack.frame_id.IsSequenceValid());

  begin_frame_tracker_.ReceivedAck(ack);

  // Override the has_damage flag (ignoring invalid data from clients).
  BeginFrameAck modified_ack(ack);
  modified_ack.has_damage = false;

  // If the client doesn't produce a frame, we assume it's no longer interactive
  // for scheduling.
  if (last_activated_surface_id_.is_valid())
    surface_manager_->SurfaceModified(last_activated_surface_id_, modified_ack,
                                      SurfaceObserver::HandleInteraction::kNo);

  if (begin_frame_source_) {
    begin_frame_source_->DidFinishFrame(this);
    frame_sink_manager_->DidFinishFrame(frame_sink_id_, last_begin_frame_args_);
  }
  if (ack.preferred_frame_interval &&
      frame_sink_type_ == mojom::CompositorFrameSinkType::kLayerTree) {
    preferred_frame_interval_ = *ack.preferred_frame_interval;
    ApplyPreferredFrameRate(ack.frame_id.source_id);
  }
}

void CompositorFrameSinkSupport::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    std::optional<HitTestRegionList> hit_test_region_list,
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

void CompositorFrameSinkSupport::SubmitCompositorFrameLocally(
    const SurfaceId& surface_id,
    CompositorFrame frame,
    const RendererSettings& settings) {
  CHECK_EQ(surface_id, last_created_surface_id_);

  pending_frames_.push_back(FrameData{.local_frame = true});
  Surface* surface = surface_manager_->GetSurfaceForId(surface_id);

  auto frame_rejected_callback = base::ScopedClosureRunner(
      base::BindOnce([] { NOTREACHED_IN_MIGRATION(); }));
  auto frame_index = ++last_frame_index_;
  Surface::QueueFrameResult result = surface->QueueFrame(
      std::move(frame), frame_index, std::move(frame_rejected_callback));
  // Currently, frames are only queued on Android, and we don't need to use
  // `SubmitCompositorFrameLocally` for evicting resources on Android.
  CHECK_EQ(result, Surface::QueueFrameResult::ACCEPTED_ACTIVE);

  // Make sure this surface will be stretched to match the display size. If
  // `auto_resize_output_surface` is false, then swap will not occur meaning
  // that the content of this compositor frame will not be presented. If it is
  // not, then we won't properly push out existing resources. A mismatch between
  // root surface size and display size can happen. For example, there is a race
  // condition if `Display` is resized after it is set not visible but before
  // any compositor frame with that new size is submitted.
  CHECK(settings.auto_resize_output_surface);
}

SubmitResult CompositorFrameSinkSupport::MaybeSubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    std::optional<HitTestRegionList> hit_test_region_list,
    uint64_t submit_time,
    mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback callback) {
  if (!client_needs_begin_frame_ && auto_needs_begin_frame_) {
    // SetNeedsBeginFrame(true) below may cause `last_begin_frame_args_` to be
    // updated.
    BeginFrameId old_frame_id = last_begin_frame_args_.frame_id;

    handling_auto_needs_begin_frame_ = true;
    SetNeedsBeginFrame(true);
    handling_auto_needs_begin_frame_ = false;

    // If the unsolicited frame has manual frame source and a new BeginFrameArgs
    // is received because of this unsolicited frame, update the frame ID to
    // to match the BeginFrameArgs, so that the corresponding surface won't be
    // considered as pending.
    if (frame.metadata.begin_frame_ack.frame_id.source_id ==
            BeginFrameArgs::kManualSourceId &&
        last_begin_frame_args_.frame_id != old_frame_id) {
      CHECK(last_begin_frame_args_.frame_id.IsSequenceValid());

      frame.metadata.begin_frame_ack.frame_id = last_begin_frame_args_.frame_id;
    }
  }

  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global((frame.metadata.begin_frame_ack.trace_id)),
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_RECEIVE_COMPOSITOR_FRAME);
        frame_sink_id_.WriteIntoTrace(ctx.Wrap(data->set_frame_sink_id()));
        data->set_display_trace_id(frame.metadata.begin_frame_ack.trace_id);
      });

  DCHECK(local_surface_id.is_valid());
  DCHECK(!frame.render_pass_list.empty());
  DCHECK(!frame.size_in_pixels().IsEmpty());

  CHECK(callback_received_begin_frame_);
  CHECK(callback_received_receive_ack_);

  begin_frame_tracker_.ReceivedAck(frame.metadata.begin_frame_ack);
  pending_frames_.push_back(FrameData{.local_frame = false});

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
  pending_received_frame_times_.emplace(
      frame.metadata.frame_token,
      std::make_unique<PendingFrameDetails>(now_time, surface_manager_));

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

  // TODO(crbug.com/40578019): It should be possible to use
  // |frame.metadata.frame_token| instead of maintaining a |last_frame_index_|.
  uint64_t frame_index = ++last_frame_index_;

  if (frame.metadata.begin_frame_ack.preferred_frame_interval) {
    preferred_frame_interval_ =
        *frame.metadata.begin_frame_ack.preferred_frame_interval;
  } else {
    preferred_frame_interval_ = BeginFrameArgs::MinInterval();
  }

  if (features::ShouldOnBeginFrameThrottleVideo() &&
      frame_sink_type_ == mojom::CompositorFrameSinkType::kVideo) {
    ApplyPreferredFrameRate(frame.metadata.begin_frame_ack.frame_id.source_id);
  }

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

    const bool has_copy_request_against_prev_surface =
        frame.metadata.screenshot_destination.has_value() && prev_surface;

    current_surface = surface_manager_->CreateSurface(
        weak_factory_.GetWeakPtr(), surface_info,
        has_copy_request_against_prev_surface ? last_created_surface_id_
                                              : SurfaceId());

    // The previous surface needs to be valid to generate a screenshot.
    //
    // NOTE: In order for the previous surface to be copied, it needs to be
    // reachable and kept alive. This is achieved by adding a reference from the
    // current surface to the previous surface. Normally this reference is
    // removed when the copy is finished in Viz. However we could run into an
    // edge case where the frame sink is destroyed before the copy is finished.
    // If that happens, we will rely on the GC of the current surface to remove
    // the reference.
    if (has_copy_request_against_prev_surface) {
      auto copy_request = std::make_unique<CopyOutputRequest>(
          CopyOutputRequest::ResultFormat::RGBA,
          CopyOutputRequest::ResultDestination::kSystemMemory,
          base::BindOnce(
              &RemoveSurfaceReferenceAndDispatchCopyOutputRequestCallback,
              frame_sink_manager_->GetWeakPtr(), surface_info.id(),
              frame.metadata.screenshot_destination.value()));
      if (const auto& size_for_testing =
              frame_sink_manager_
                  ->copy_output_request_result_size_for_testing();  // IN-TEST
          !size_for_testing.IsEmpty()) [[unlikely]] {
        SetCopyOutoutRequestResultSize(copy_request.get(), gfx::Rect(),
                                       size_for_testing,
                                       prev_surface->size_in_pixels());
      }

      copy_request->set_result_task_runner(
          base::SequencedTaskRunner::GetCurrentDefault());

      RequestCopyOfOutput(
          PendingCopyOutputRequest(last_created_surface_id_.local_surface_id(),
                                   SubtreeCaptureId{}, std::move(copy_request),
                                   /*capture_exact_id=*/true));
    }

    if (!current_surface) {
      TRACE_EVENT_INSTANT0("viz", "Surface belongs to another client",
                           TRACE_EVENT_SCOPE_THREAD);
      return SubmitResult::SURFACE_OWNED_BY_ANOTHER_CLIENT;
    }
    last_created_surface_id_ = SurfaceId(frame_sink_id_, local_surface_id);

    surface_manager_->SurfaceDamageExpected(current_surface->surface_id(),
                                            last_begin_frame_args_);
  }

  // We use `last_created_surface_id_` to use the embedding timestamp of the
  // last Surface which was successfully presented before this point, in case
  // the frame is rejected. Now that we know this frame can be presented, use
  // its correct SurfaceId.
  pending_received_frame_times_[frame.metadata.frame_token]->set_surface_id(
      last_created_surface_id_);

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
  DCHECK(!pending_frames_.empty());
  bool was_pending_manual_begin_frame_source_ =
      pending_manual_begin_frame_source_;

  // TODO(https://crbug.com/40902503): Drawing from a layer context is indeed
  // local, but we'll likely want to use a different resource return policy.
  bool was_local_frame = pending_frames_.front().local_frame || layer_context_;
  pending_frames_.pop_front();

  if (pending_frames_.empty()) {
    pending_manual_begin_frame_source_ = false;
  }
  if (!client_)
    return;

  // If this frame came from viz directly and not from the client, don't send
  // the client an ack, since it didn't do anything. Just return the resources.
  if (was_local_frame) {
    client_->ReclaimResources(std::move(surface_returned_resources_));
    surface_returned_resources_.clear();
    return;
  }

  // If we have a callback, we only return the resource on onBeginFrame.
  if (compositor_frame_callback_) {
    callback_received_receive_ack_ = true;
    UpdateNeedsBeginFramesInternal();
    HandleCallback();
    return;
  }

  // When we want to merge OnBeginFrame signals with Acks, we want to enqueue
  // the Ack here, and exit. An exception to this are when the frame was
  // submitted with a manual BeginFrameSource, as that is driven by the client,
  // and not by our `begin_frame_source_`.
  //
  // The other exception is when we have just sent an OnBeginFrame, however
  // there was an Ack pending at that time. This typically occurs when a client
  // submits a frame right before the next VSync. In this case we do want to
  // send a separate Ack, so they can unthrottle and begin frame production.
  if (ShouldMergeBeginFrameWithAcks() &&
      !was_pending_manual_begin_frame_source_ &&
      !ack_pending_during_on_begin_frame_) {
    ack_queued_for_client_count_++;
    UpdateNeedsBeginFramesInternal();
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
  CHECK_NE(frame_token, kInvalidFrameToken);
  CHECK_NE(frame_token, kLocalFrameToken);
  DCHECK((feedback.flags & gfx::PresentationFeedback::kFailure) ||
         (!draw_start_timestamp.is_null() && !swap_timings.is_null()));

  DCHECK_LE(pending_received_frame_times_.size(), 25u);
  auto received_frame_timestamp =
      pending_received_frame_times_.find(frame_token);
  CHECK(received_frame_timestamp != pending_received_frame_times_.end());

  FrameTimingDetails details;
  details.received_compositor_frame_timestamp =
      received_frame_timestamp->second->frame_submit_timestamp();
  details.embedded_frame_timestamp =
      is_root_ ? details.received_compositor_frame_timestamp
               : received_frame_timestamp->second->frame_embed_timestamp();
  details.draw_start_timestamp = draw_start_timestamp;
  details.swap_timings = swap_timings;
  details.presentation_feedback = feedback;
  AdjustPresentationFeedback(&details.presentation_feedback,
                             swap_timings.swap_start);
  // Override with the throttled interval if one has been set. Otherwise,
  // consumers will assume that the default vsync interval was the target and
  // that the frames are presented too late when in fact, this is intentional.
  if (begin_frame_interval_.is_positive() &&
      details.presentation_feedback.interval.is_positive()) {
    details.presentation_feedback.interval = begin_frame_interval_;
  }
  pending_received_frame_times_.erase(received_frame_timestamp);

  // We should only ever get one PresentationFeedback per frame_token.
  CHECK(!frame_timing_details_.contains(frame_token));
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
  int64_t trace_id = ComputeTraceId();
  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(trace_id), [trace_id](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_ISSUE_BEGIN_FRAME);
        data->set_display_trace_id(trace_id);
      });

  if (compositor_frame_callback_) {
    callback_received_begin_frame_ = true;
    UpdateNeedsBeginFramesInternal();
    HandleCallback();
  }

  CheckPendingSurfaces();

  BeginFrameArgs adjusted_args = args;
  adjusted_args.dispatch_time = base::TimeTicks::Now();
  if (begin_frame_interval_.is_positive()) {
    adjusted_args.interval = begin_frame_interval_;
    // Deadline is not necessarily frame_time + interval. For example, it may
    // incorporate an estimate for the frame's draw/swap time, so it's
    // desirable to preserve any offset from the next scheduled frame.
    base::TimeDelta offset_from_next_scheduled_frame =
        args.deadline - (args.frame_time + args.interval);
    adjusted_args.deadline = args.frame_time + begin_frame_interval_ +
                             offset_from_next_scheduled_frame;
  }
  SetLastKnownVsync(args.interval);
  const bool should_send_begin_frame =
      ShouldSendBeginFrame(adjusted_args.frame_time, args.interval) &&
      (client_ || layer_context_);
  if (should_send_begin_frame) {
    if (last_activated_surface_id_.is_valid())
      surface_manager_->SurfaceDamageExpected(last_activated_surface_id_,
                                              adjusted_args);
    last_begin_frame_args_ = adjusted_args;

    // Force full frame if surface not yet activated to ensure surface creation.
    if (!last_activated_surface_id_.is_valid())
      adjusted_args.animate_only = false;

    adjusted_args.trace_id = trace_id;
    adjusted_args.frames_throttled_since_last = frames_throttled_since_last_;
    frames_throttled_since_last_ = 0;

    last_frame_time_ = adjusted_args.frame_time;

    if (client_) {
      if (ShouldMergeBeginFrameWithAcks()) {
        bool frame_ack = ack_queued_for_client_count_ > 0;
        ack_pending_during_on_begin_frame_ =
            !frame_ack && !pending_frames_.empty();

        // No need to send a BeginFrame request immediately to the client if
        // this OnBeginFrame() call is triggered by an unsolicited frame in the
        // AutoNeedsBeginFrame mode.
        if (!handling_auto_needs_begin_frame_) {
          client_->OnBeginFrame(adjusted_args, frame_timing_details_, frame_ack,
                                std::move(surface_returned_resources_));
          frame_timing_details_.clear();
        } else {
          if (frame_ack) {
            client_->DidReceiveCompositorFrameAck(
                std::move(surface_returned_resources_));
          } else if (!surface_returned_resources_.empty()) {
            client_->ReclaimResources(std::move(surface_returned_resources_));
          }
        }

        if (frame_ack) {
          ack_queued_for_client_count_--;
        }
        surface_returned_resources_.clear();
      } else if (!handling_auto_needs_begin_frame_) {
        client_->OnBeginFrame(adjusted_args, frame_timing_details_,
                              /*frame_ack=*/false,
                              std::vector<ReturnedResource>());
        frame_timing_details_.clear();
      }
    }

    begin_frame_tracker_.SentBeginFrame(adjusted_args);
    frame_sink_manager_->DidBeginFrame(frame_sink_id_, adjusted_args);
    UpdateNeedsBeginFramesInternal();

    if (layer_context_) {
      // NOTE: BeginFrame() re-enters `this` to finish the frame.
      layer_context_->BeginFrame(adjusted_args);
      frame_timing_details_.clear();
    }
  } else if (begin_frame_source_) {
    begin_frame_source_->DidFinishFrame(this);
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
  // if the client is waiting for a frame ack, or if there are resources to
  // return.
  needs_begin_frame_ =
      (client_needs_begin_frame_ || !frame_timing_details_.empty() ||
       !pending_surfaces_.empty() || layer_context_wants_begin_frames_ ||
       (compositor_frame_callback_ && !callback_received_begin_frame_) ||
       (ShouldMergeBeginFrameWithAcks() &&
        (!surface_returned_resources_.empty() ||
         ack_queued_for_client_count_)));

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
}

void CompositorFrameSinkSupport::StopObservingBeginFrameSource() {
  added_frame_observer_ = false;
  begin_frame_source_->RemoveObserver(this);
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

std::optional<CapturableFrameSink::RegionProperties>
CompositorFrameSinkSupport::GetRequestRegionProperties(
    const VideoCaptureSubTarget& sub_target) const {
  if (!last_activated_surface_id_.is_valid())
    return {};

  Surface* current_surface =
      surface_manager_->GetSurfaceForId(last_activated_surface_id_);
  DCHECK(current_surface);
  if (!current_surface->HasActiveFrame()) {
    return {};
  }

  const CompositorFrame& frame = current_surface->GetActiveFrame();
  RegionProperties out;
  out.root_render_pass_size = frame.size_in_pixels();
  if (out.root_render_pass_size.IsEmpty()) {
    return {};
  }

  // If we don't have a sub target, capture everything in the frame.
  if (IsEntireTabCapture(sub_target)) {
    out.render_pass_subrect = gfx::Rect(out.root_render_pass_size);
    return out;
  }

  // If we have a region capture crop ID, capture a subsection of the root
  // render pass.
  if (IsRegionCapture(sub_target)) {
    const auto it = current_capture_bounds_.bounds().find(
        absl::get<RegionCaptureCropId>(sub_target));
    if (it != current_capture_bounds_.bounds().end() && !it->second.IsEmpty() &&
        gfx::Rect(out.root_render_pass_size).Contains(it->second)) {
      out.render_pass_subrect = it->second;
      return out;
    }

    // Nothing to capture.
    return {};
  }

  // Else, we have a subtree capture ID and should capture a subsection of a
  // child render pass.
  CHECK(IsSubtreeCapture(sub_target));
  const SubtreeCaptureId& id = absl::get<SubtreeCaptureId>(sub_target);
  for (const auto& render_pass : frame.render_pass_list) {
    if (render_pass->subtree_capture_id == id) {
      out.transform_to_root = render_pass->transform_to_root_target;

      if (!render_pass->subtree_size.IsEmpty()) {
        out.render_pass_subrect = gfx::Rect(render_pass->subtree_size);
      } else {
        out.render_pass_subrect =
            IntersectInSpace(render_pass->output_rect, out.transform_to_root,
                             gfx::Rect(out.root_render_pass_size));
      }

      if (!out.render_pass_subrect.IsEmpty() &&
          render_pass->output_rect.Contains(out.render_pass_subrect)) {
        return out;
      }
    }
  }

  // No target exists and no CopyOutputRequest will be added.
  return {};
}

void CompositorFrameSinkSupport::RequestCopyOfOutput(
    PendingCopyOutputRequest pending_copy_output_request) {
  copy_output_requests_.push_back(std::move(pending_copy_output_request));
  if (last_activated_surface_id_.is_valid()) {
    BeginFrameAck ack;
    ack.has_damage = true;
    surface_manager_->SurfaceModified(
        last_activated_surface_id_, ack,
        SurfaceObserver::HandleInteraction::kNoChange);
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
  NOTREACHED_IN_MIGRATION();
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
    base::TimeTicks frame_time,
    base::TimeDelta vsync_interval) {
  // We should throttle OnBeginFrame() if it has been less than
  // |begin_frame_interval_| since the last one was sent because clients have
  // requested to update at such rate.
  const bool should_throttle_as_requested =
      ShouldThrottleBeginFrameAsRequested(frame_time, vsync_interval);
  // We might throttle this OnBeginFrame() if it's been less than a second
  // since the last one was sent, either because clients are unresponsive or
  // have submitted too many undrawn frames.
  const bool can_throttle_if_unresponsive_or_excessive =
      frame_time - last_frame_time_ < base::Seconds(1);

  bool should_throttle_undrawn_frames = false;
  if (last_activated_surface_id_.is_valid()) {
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
    should_throttle_undrawn_frames =
        can_throttle_if_unresponsive_or_excessive &&
        num_undrawn_frames > kUndrawnFrameLimit &&
        surface->GetActiveFrameMetadata().may_throttle_if_undrawn_frames;
  }

  bool frame_timing_details_all_failed = true;
  for (const auto& entry : frame_timing_details_) {
    if (!entry.second.presentation_feedback.failed()) {
      frame_timing_details_all_failed = false;
      break;
    }
  }

  // If there are pending timing details from the previous successfully
  // presented frame(s), then the non-throttled client needs to receive the
  // begin-frame.
  if (!frame_timing_details_.empty() && !should_throttle_as_requested &&
      (!should_throttle_undrawn_frames || !frame_timing_details_all_failed)) {
    return RecordShouldSendBeginFrame("SendFrameTiming", true);
  }

  // If the client is waiting for an ack from a previously submitted frame, then
  // the client needs to receive the begin-frame.
  if (ack_queued_for_client_count_ && !should_throttle_as_requested) {
    return RecordShouldSendBeginFrame("SendFrameAck", true);
  }

  if (!client_needs_begin_frame_ && !layer_context_wants_begin_frames_) {
    return RecordShouldSendBeginFrame("StopNotRequested", false);
  }

  // Stop sending BeginFrames to clients that are totally unresponsive.
  if (begin_frame_tracker_.ShouldStopBeginFrame()) {
    return RecordShouldSendBeginFrame("StopUnresponsiveClient", false);
  }

  // Throttle clients that are unresponsive.
  if (can_throttle_if_unresponsive_or_excessive &&
      begin_frame_tracker_.ShouldThrottleBeginFrame()) {
    return RecordShouldSendBeginFrame("ThrottleUnresponsiveClient", false);
  }

  if (!last_activated_surface_id_.is_valid()) {
    return RecordShouldSendBeginFrame("SendNoActiveSurface", true);
  }

  // We should never throttle BeginFrames if there is another client waiting
  // for this client to submit a frame.
  if (surface_manager_->HasBlockedEmbedder(frame_sink_id_)) {
    return RecordShouldSendBeginFrame("SendBlockedEmbedded", true);
  }

  if (should_throttle_as_requested) {
    ++frames_throttled_since_last_;
    return RecordShouldSendBeginFrame("ThrottleRequested", false);
  }

  if (should_throttle_undrawn_frames) {
    return RecordShouldSendBeginFrame("ThrottleUndrawnFrames", false);
  }

  // No other conditions apply so send the begin frame.
  return RecordShouldSendBeginFrame("SendDefault", true);
}

void CompositorFrameSinkSupport::CheckPendingSurfaces() {
  if (pending_surfaces_.empty())
    return;
  base::flat_set<raw_ptr<Surface, CtnExperimental>> pending_surfaces(
      pending_surfaces_);
  for (Surface* surface : pending_surfaces) {
    surface->ActivateIfDeadlinePassed();
  }
}

bool CompositorFrameSinkSupport::ShouldMergeBeginFrameWithAcks() const {
  return features::IsOnBeginFrameAcksEnabled() && wants_begin_frame_acks_ &&
         !layer_context_;
}

bool CompositorFrameSinkSupport::ShouldThrottleBeginFrameAsRequested(
    base::TimeTicks frame_time,
    base::TimeDelta vsync_interval) {
  if (!begin_frame_interval_.is_positive() || !vsync_interval.is_positive()) {
    return false;
  }

  // At this point, throttling is in place and following a simple cadence, we
  // need to know if the current frame has elapsed a full cadence interval to
  // know its time to render.
  base::TimeDelta time_since_last_frame = frame_time - last_frame_time_;
  return !HasElapsedCadenceInterval(vsync_interval, begin_frame_interval_,
                                    time_since_last_frame);
}

void CompositorFrameSinkSupport::ProcessCompositorFrameTransitionDirective(
    const CompositorFrameTransitionDirective& directive,
    Surface* surface) {
  const auto& transition_token = directive.transition_token();

  switch (directive.type()) {
    case CompositorFrameTransitionDirective::Type::kSave:
      // The save directive is used to start a new transition sequence. Ensure
      // we don't have any existing state for this transition.
      if (frame_sink_manager_->ClearSurfaceAnimationManager(transition_token))
          [[unlikely]] {
        view_transition_token_to_animation_manager_.erase(transition_token);
        return;
      }
      if (view_transition_token_to_animation_manager_.erase(transition_token))
          [[unlikely]] {
        return;
      }

      view_transition_token_to_animation_manager_[transition_token] =
          SurfaceAnimationManager::CreateWithSave(
              directive, surface, frame_sink_manager_->shared_bitmap_manager(),
              use_blit_request_for_view_transition_
                  ? frame_sink_manager_->GetSharedImageInterface()
                  : nullptr,
              frame_sink_manager_->reserved_resource_id_tracker(),
              base::BindOnce(&CompositorFrameSinkSupport::
                                 OnSaveTransitionDirectiveProcessed,
                             base::Unretained(this)));
      break;
    case CompositorFrameTransitionDirective::Type::kAnimateRenderer: {
      if (directive.maybe_cross_frame_sink()) {
        // We shouldn't have an existing SurfaceAnimationManager for this
        // token.
        if (view_transition_token_to_animation_manager_.erase(
                transition_token)) {
          return;
        }
        view_transition_token_to_animation_manager_[transition_token] =
            frame_sink_manager_->TakeSurfaceAnimationManager(transition_token);
      }

      auto it =
          view_transition_token_to_animation_manager_.find(transition_token);
      if (it == view_transition_token_to_animation_manager_.end()) {
        return;
      }

      // The save operation must have been completed before the renderer sends
      // an animate directive.
      auto& surface_animation_manager = it->second;
      if (!surface_animation_manager->Animate()) {
        view_transition_token_to_animation_manager_.erase(it);
        return;
      }
    } break;
    case CompositorFrameTransitionDirective::Type::kRelease:
      frame_sink_manager_->ClearSurfaceAnimationManager(
          directive.transition_token());
      view_transition_token_to_animation_manager_.erase(
          directive.transition_token());
      break;
  }
}

void CompositorFrameSinkSupport::OnSaveTransitionDirectiveProcessed(
    const CompositorFrameTransitionDirective& directive) {
  DCHECK_EQ(directive.type(), CompositorFrameTransitionDirective::Type::kSave)
      << "Only save directives need to be ack'd back to the client";

  auto it = view_transition_token_to_animation_manager_.find(
      directive.transition_token());
  if (it == view_transition_token_to_animation_manager_.end()) {
    return;
  }

  CHECK(it->second);

  if (client_) {
    client_->OnCompositorFrameTransitionDirectiveProcessed(
        directive.sequence_id());
  }

  if (directive.maybe_cross_frame_sink()) {
    frame_sink_manager_->CacheSurfaceAnimationManager(
        directive.transition_token(), std::move(it->second));
    view_transition_token_to_animation_manager_.erase(it);
  }
}

bool CompositorFrameSinkSupport::IsEvicted(
    const LocalSurfaceId& local_surface_id) const {
  return local_surface_id.embed_token() ==
             last_evicted_local_surface_id_.embed_token() &&
         local_surface_id.parent_sequence_number() <=
             last_evicted_local_surface_id_.parent_sequence_number();
}

void CompositorFrameSinkSupport::ClearAllPendingCopyOutputRequests() {
  CHECK(surface_manager_);
  for (auto& request : copy_output_requests_) {
    // If the frame sink is getting destroyed while there are still
    // outstanding `CopyOutputRequest`s to capture an associated surface,
    // transfer these requests to the corresponding `Surface`s.
    //
    // Resources reclamation: once frame sink is destroyed, the `Surface`s
    // won't be able to notify the client code (the renderer's
    // `cc::LayerTreeHostImpl`) to reclaim the resources. This is fine,
    // because the destruction of the renderer and its CC (as part of a
    // cross-RenderFrame navigation) will implicitly reclaim all the
    // resources. The `Surface` kept alive will still have a reference to
    // the underlying GPU resources. The GPU resources will finally be
    // released when the `Surface` is destroyed (in this case, after the
    // CopyOutputRequest is fulfilled).
    if (request.capture_exact_surface_id) {
      const SurfaceId target_id(frame_sink_id_, request.local_surface_id);
      auto* target_surface = surface_manager_->GetSurfaceForId(target_id);
      if (target_surface) {
        target_surface->RequestCopyOfOutput(std::move(request));

        BeginFrameAck ack;
        ack.has_damage = true;
        surface_manager_->SurfaceModified(
            target_id, ack, SurfaceObserver::HandleInteraction::kNoChange);
      } else {
        // We might not have a `Surface` if the renderer is crashed, or too busy
        // to even submit a CompositorFrame (e.g., low end Android devices). In
        // either cases we don't want to crash the GPU in production. The
        // WARNING logs will show up in "chrome://gpu".
        LOG(WARNING) << "Surface " << target_id
                     << " is destroyed while there is an outstanding "
                        "CopyOutputRequest specificc for it.";
      }
    } else {
      // TODO(crbug.com/40276723): We should probably transfer all
      // the requests to their corresponding `Surface`s or `RenderPass`es.
    }
  }
  // Upon destruction, the `PendingOutputRequest` will invoke the callback to
  // return an empty bitmap, signalling that the request is never satisfied.
  copy_output_requests_.clear();
}

void CompositorFrameSinkSupport::SetExternalReservedResourceDelegate(
    ReservedResourceDelegate* delegate) {
  external_reserved_resource_delegate_ = delegate;
}

void CompositorFrameSinkSupport::SetLayerContextWantsBeginFrames(
    bool wants_begin_frames) {
  layer_context_wants_begin_frames_ = wants_begin_frames;
  UpdateNeedsBeginFramesInternal();
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

void CompositorFrameSinkSupport::ForAllReservedResourceDelegates(
    base::FunctionRef<void(ReservedResourceDelegate&)> func) {
  if (external_reserved_resource_delegate_) {
    func(*external_reserved_resource_delegate_);
  }

  for (auto& it : view_transition_token_to_animation_manager_) {
    func(*it.second);
  }
}

CompositorFrameSinkSupport::PendingFrameDetails::PendingFrameDetails(
    base::TimeTicks frame_submit_timestamp,
    SurfaceManager* surface_manager)
    : frame_submit_timestamp_(frame_submit_timestamp),
      // Use the submit timestamp as the default value, so that the metrics
      // won't get skewed in case the surface never gets embedded/the surface
      // ID never gets set.
      frame_embed_timestamp_(frame_submit_timestamp),
      surface_manager_(surface_manager) {}

void CompositorFrameSinkSupport::PendingFrameDetails::
    SetOrObserveFrameEmbedTimeStamp() {
  frame_embed_timestamp_ =
      surface_manager_->GetSurfaceReferencedTimestamp(surface_id_);
  if (frame_embed_timestamp_ == base::TimeTicks()) {
    // The frame hasn't been embedded yet. Observe `OnAddedSurfaceReference()`
    // to be notified when the surface for the frame gets embedded.
    surface_manager_->AddObserver(this);
  }
}

CompositorFrameSinkSupport::PendingFrameDetails::~PendingFrameDetails() {
  surface_manager_->RemoveObserver(this);
}

void CompositorFrameSinkSupport::PendingFrameDetails::set_surface_id(
    SurfaceId surface_id) {
  CHECK(!surface_id_.is_valid());
  surface_id_ = surface_id;
  SetOrObserveFrameEmbedTimeStamp();
}

void CompositorFrameSinkSupport::PendingFrameDetails::OnAddedSurfaceReference(
    const SurfaceId& parent_id,
    const SurfaceId& child_id) {
  CHECK_EQ(frame_embed_timestamp_, base::TimeTicks());
  if (child_id != surface_id_) {
    return;
  }
  frame_embed_timestamp_ = base::TimeTicks::Now();
  surface_manager_->RemoveObserver(this);
}

}  // namespace viz
