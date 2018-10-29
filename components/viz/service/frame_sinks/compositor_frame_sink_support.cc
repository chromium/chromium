// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"

#include <algorithm>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_reference.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace viz {

CompositorFrameSinkSupport::CompositorFrameSinkSupport(
    mojom::CompositorFrameSinkClient* client,
    FrameSinkManagerImpl* frame_sink_manager,
    const FrameSinkId& frame_sink_id,
    bool is_root,
    bool needs_sync_tokens)
    : client_(client),
      frame_sink_manager_(frame_sink_manager),
      surface_manager_(frame_sink_manager->surface_manager()),
      frame_sink_id_(frame_sink_id),
      surface_resource_holder_(this),
      is_root_(is_root),
      needs_sync_tokens_(needs_sync_tokens),
      allow_copy_output_requests_(is_root),
      weak_factory_(this) {
  // This may result in SetBeginFrameSource() being called.
  frame_sink_manager_->RegisterCompositorFrameSinkSupport(frame_sink_id_, this);
}

CompositorFrameSinkSupport::~CompositorFrameSinkSupport() {
  // Unregister |this| as a BeginFrameObserver so that the
  // BeginFrameSource does not call into |this| after it's deleted.
  callback_received_begin_frame_ = true;
  callback_received_receive_ack_ = true;
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
    surface_manager_->DestroySurface(last_created_surface_id_);
  frame_sink_manager_->UnregisterCompositorFrameSinkSupport(frame_sink_id_);

  // The display compositor has ownership of shared memory for each
  // SharedBitmapId that has been reported from the client. Since the client is
  // gone that memory can be freed. If we don't then it would leak.
  for (const auto& id : owned_bitmaps_)
    frame_sink_manager_->shared_bitmap_manager()->ChildDeletedSharedBitmap(id);

  // No video capture clients should remain after calling
  // UnregisterCompositorFrameSinkSupport().
  DCHECK(capture_clients_.empty());
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

void CompositorFrameSinkSupport::OnSurfaceActivated(Surface* surface) {
  DCHECK(surface);
  DCHECK(surface->HasActiveFrame());

  const LocalSurfaceId& local_surface_id =
      surface->surface_id().local_surface_id();
  const LocalSurfaceId& last_activated_local_surface_id =
      last_activated_surface_id_.local_surface_id();

  if (!last_activated_surface_id_.is_valid() ||
      local_surface_id > last_activated_local_surface_id) {
    if (last_activated_surface_id_.is_valid()) {
      CHECK_GE(local_surface_id.parent_sequence_number(),
               last_activated_local_surface_id.parent_sequence_number());
      CHECK_GE(local_surface_id.child_sequence_number(),
               last_activated_local_surface_id.child_sequence_number());

      Surface* prev_surface =
          surface_manager_->GetSurfaceForId(last_activated_surface_id_);
      DCHECK(prev_surface);
      surface->SetPreviousFrameSurface(prev_surface);
      surface_manager_->DestroySurface(prev_surface->surface_id());
    }
    last_activated_surface_id_ = surface->surface_id();
  } else if (surface->surface_id() < last_activated_surface_id_) {
    // We can get into a situation where a child-initiated synchronization is
    // deferred until after a parent-initiated synchronization happens resulting
    // in activations happening out of order. In that case, we simply discard
    // the stale surface.
    surface_manager_->DestroySurface(surface->surface_id());
  }

  DCHECK(surface->HasActiveFrame());

  // Check if this is a display root surface and the SurfaceId is changing.
  if (is_root_ && (!referenced_local_surface_id_ ||
                   *referenced_local_surface_id_ !=
                       last_activated_surface_id_.local_surface_id())) {
    UpdateDisplayRootReference(surface);
  }

  MaybeEvictSurfaces();
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

void CompositorFrameSinkSupport::OnSurfaceDiscarded(Surface* surface) {
  if (surface->surface_id() == last_activated_surface_id_)
    last_activated_surface_id_ = SurfaceId();

  if (surface->surface_id() == last_created_surface_id_)
    last_created_surface_id_ = SurfaceId();
}

void CompositorFrameSinkSupport::RefResources(
    const std::vector<TransferableResource>& resources) {
  surface_resource_holder_.RefResources(resources);
}

void CompositorFrameSinkSupport::UnrefResources(
    const std::vector<ReturnedResource>& resources) {
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

std::vector<std::unique_ptr<CopyOutputRequest>>
CompositorFrameSinkSupport::TakeCopyOutputRequests(
    const LocalSurfaceId& latest_local_id) {
  std::vector<std::unique_ptr<CopyOutputRequest>> results;
  for (auto it = copy_output_requests_.begin();
       it != copy_output_requests_.end();) {
    // Requests with a non-valid local id should be satisfied as soon as
    // possible.
    if (!it->first.is_valid() || it->first <= latest_local_id) {
      results.push_back(std::move(it->second));
      it = copy_output_requests_.erase(it);
    } else {
      ++it;
    }
  }
  return results;
}

void CompositorFrameSinkSupport::EvictSurface(const LocalSurfaceId& id) {
  DCHECK_GE(id.parent_sequence_number(), last_evicted_parent_sequence_number_);
  last_evicted_parent_sequence_number_ = id.parent_sequence_number();
  MaybeEvictSurfaces();
}

void CompositorFrameSinkSupport::MaybeEvictSurfaces() {
  if (last_activated_surface_id_.is_valid() &&
      last_activated_surface_id_.local_surface_id().parent_sequence_number() <=
          last_evicted_parent_sequence_number_) {
    EvictLastActiveSurface();
  }
  if (last_created_surface_id_.is_valid() &&
      last_created_surface_id_.local_surface_id().parent_sequence_number() <=
          last_evicted_parent_sequence_number_) {
    surface_manager_->DestroySurface(last_created_surface_id_);
    last_created_surface_id_ = SurfaceId();
  }
}

void CompositorFrameSinkSupport::EvictLastActiveSurface() {
  SurfaceId to_destroy_surface_id = last_activated_surface_id_;
  if (last_created_surface_id_ == last_activated_surface_id_)
    last_created_surface_id_ = SurfaceId();
  last_activated_surface_id_ = SurfaceId();
  surface_manager_->DestroySurface(to_destroy_surface_id);

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

void CompositorFrameSinkSupport::DidNotProduceFrame(const BeginFrameAck& ack) {
  TRACE_EVENT2("viz", "CompositorFrameSinkSupport::DidNotProduceFrame",
               "ack.source_id", ack.source_id, "ack.sequence_number",
               ack.sequence_number);
  DCHECK_GE(ack.sequence_number, BeginFrameArgs::kStartingFrameNumber);

  // Override the has_damage flag (ignoring invalid data from clients).
  BeginFrameAck modified_ack(ack);
  modified_ack.has_damage = false;

  if (last_activated_surface_id_.is_valid())
    surface_manager_->SurfaceModified(last_activated_surface_id_, modified_ack);

  if (begin_frame_source_)
    begin_frame_source_->DidFinishFrame(this);
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
    mojo::ScopedSharedBufferHandle buffer,
    const SharedBitmapId& id) {
  if (!frame_sink_manager_->shared_bitmap_manager()->ChildAllocatedSharedBitmap(
          std::move(buffer), id))
    return false;

  owned_bitmaps_.insert(id);
  return true;
}

void CompositorFrameSinkSupport::DidDeleteSharedBitmap(
    const SharedBitmapId& id) {
  frame_sink_manager_->shared_bitmap_manager()->ChildDeletedSharedBitmap(id);
  owned_bitmaps_.erase(id);
}

SubmitResult CompositorFrameSinkSupport::MaybeSubmitCompositorFrameInternal(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    base::Optional<HitTestRegionList> hit_test_region_list,
    uint64_t submit_time,
    mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback callback) {
  TRACE_EVENT1("viz", "CompositorFrameSinkSupport::MaybeSubmitCompositorFrame",
               "FrameSinkId", frame_sink_id_.ToString());

  TRACE_EVENT_WITH_FLOW1(
      "viz,benchmark", "Graphics.Pipeline",
      TRACE_ID_GLOBAL(frame.metadata.begin_frame_ack.trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "ReceiveCompositorFrame");

  TRACE_EVENT_FLOW_END0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
                        "SubmitCompositorFrame", local_surface_id.hash());

  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
                                     &tracing_enabled);
  if (tracing_enabled) {
    base::TimeDelta elapsed = base::TimeTicks::Now().since_origin() -
                              base::TimeDelta::FromMicroseconds(submit_time);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
                         "SubmitCompositorFrame::TimeElapsed",
                         TRACE_EVENT_SCOPE_THREAD,
                         "elapsed time:", elapsed.InMicroseconds());
  }

  DCHECK(local_surface_id.is_valid());
  DCHECK(!frame.render_pass_list.empty());
  DCHECK(!frame.size_in_pixels().IsEmpty());

  CHECK(callback_received_begin_frame_);
  CHECK(callback_received_receive_ack_);

  ++ack_pending_count_;

  base::ScopedClosureRunner frame_rejected_callback(base::BindOnce(
      &CompositorFrameSinkSupport::DidRejectCompositorFrame,
      weak_factory_.GetWeakPtr(), frame.metadata.frame_token,
      frame.metadata.request_presentation_feedback, frame.resource_list));

  compositor_frame_callback_ = std::move(callback);
  if (compositor_frame_callback_) {
    callback_received_begin_frame_ = false;
    callback_received_receive_ack_ = false;
    UpdateNeedsBeginFramesInternal();
  }

  // Ensure no CopyOutputRequests have been submitted if they are banned.
  if (!allow_copy_output_requests_ && frame.HasCopyOutputRequests()) {
    TRACE_EVENT_INSTANT0("viz", "CopyOutputRequests not allowed",
                         TRACE_EVENT_SCOPE_THREAD);
    return SubmitResult::COPY_OUTPUT_REQUESTS_NOT_ALLOWED;
  }

  // TODO(crbug.com/846739): It should be possible to use
  // |frame.metadata.frame_token| instead of maintaining a |last_frame_index_|.
  uint64_t frame_index = ++last_frame_index_;

  // Override the has_damage flag (ignoring invalid data from clients).
  frame.metadata.begin_frame_ack.has_damage = true;
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber,
            frame.metadata.begin_frame_ack.sequence_number);

  if (!ui::LatencyInfo::Verify(frame.metadata.latency_info,
                               "RenderWidgetHostImpl::OnSwapCompositorFrame")) {
    std::vector<ui::LatencyInfo>().swap(frame.metadata.latency_info);
  }
  for (ui::LatencyInfo& latency : frame.metadata.latency_info) {
    if (latency.latency_components().size() > 0) {
      latency.AddLatencyNumber(ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT);
    }
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
    bool last_surface_has_dependent_frame =
        prev_surface && prev_surface->HasDependentFrame();

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

    if (!surface_info.is_valid() || !monotonically_increasing_id) {
      TRACE_EVENT_INSTANT0("viz", "Surface Invariants Violation",
                           TRACE_EVENT_SCOPE_THREAD);
      return SubmitResult::SURFACE_INVARIANTS_VIOLATION;
    }

    // If the last Surface doesn't have a dependent frame, and this frame
    // corresponds to a child-initiated synchronization event then defer this
    // Surface until a dependent frame arrives. This throttles child submission
    // of CompositorFrames to the parent's embedding rate.
    const bool block_activation_on_parent =
        child_initiated_synchronization_event &&
        !last_surface_has_dependent_frame;

    current_surface = CreateSurface(surface_info, block_activation_on_parent);
    last_created_surface_id_ = SurfaceId(frame_sink_id_, local_surface_id);
    MaybeEvictSurfaces();
    // If the surface was immediately evicted, don't accept the CompositorFrame.
    if (!last_created_surface_id_.is_valid()) {
      TRACE_EVENT_INSTANT0("viz", "Submit rejected to evicted surface",
                           TRACE_EVENT_SCOPE_THREAD);
      return SubmitResult::ACCEPTED;
    }

    if (!current_surface) {
      TRACE_EVENT_INSTANT0("viz", "Surface Invariants Violation",
                           TRACE_EVENT_SCOPE_THREAD);
      return SubmitResult::SURFACE_INVARIANTS_VIOLATION;
    }

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

  bool result = current_surface->QueueFrame(
      std::move(frame), frame_index, std::move(frame_rejected_callback),
      frame.metadata.request_presentation_feedback
          ? base::BindOnce(
                &CompositorFrameSinkSupport::DidPresentCompositorFrame,
                weak_factory_.GetWeakPtr(), frame.metadata.frame_token)
          : Surface::PresentedCallback());
  if (!result) {
    TRACE_EVENT_INSTANT0("viz", "QueueFrame failed", TRACE_EVENT_SCOPE_THREAD);
    return SubmitResult::SURFACE_INVARIANTS_VIOLATION;
  }

  if (begin_frame_source_)
    begin_frame_source_->DidFinishFrame(this);

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
    uint32_t presentation_token,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(presentation_token);
  if (client_) {
    client_->DidPresentCompositorFrame(presentation_token, feedback);
  }
}

void CompositorFrameSinkSupport::DidRejectCompositorFrame(
    uint32_t presentation_token,
    bool request_presentation_feedback,
    std::vector<TransferableResource> frame_resource_list) {
  std::vector<ReturnedResource> resources =
      TransferableResource::ReturnResources(frame_resource_list);
  ReturnResources(resources);
  DidReceiveCompositorFrameAck();
  if (request_presentation_feedback) {
    DidPresentCompositorFrame(presentation_token,
                              gfx::PresentationFeedback::Failure());
  }
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
  if (last_activated_surface_id_.is_valid())
    surface_manager_->SurfaceDamageExpected(last_activated_surface_id_, args);
  last_begin_frame_args_ = args;

  if (compositor_frame_callback_) {
    callback_received_begin_frame_ = true;
    UpdateNeedsBeginFramesInternal();
    HandleCallback();
  }

  if (client_ && client_needs_begin_frame_) {
    BeginFrameArgs copy_args = args;
    copy_args.trace_id = ComputeTraceId();
    TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                           TRACE_ID_GLOBAL(copy_args.trace_id),
                           TRACE_EVENT_FLAG_FLOW_OUT, "step",
                           "IssueBeginFrame");
    client_->OnBeginFrame(copy_args);
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
  // requested it.
  bool needs_begin_frame =
      client_needs_begin_frame_ ||
      (compositor_frame_callback_ && !callback_received_begin_frame_);

  if (needs_begin_frame == added_frame_observer_)
    return;

  added_frame_observer_ = needs_begin_frame;
  if (needs_begin_frame)
    begin_frame_source_->AddObserver(this);
  else
    begin_frame_source_->RemoveObserver(this);
}

Surface* CompositorFrameSinkSupport::CreateSurface(
    const SurfaceInfo& surface_info,
    bool block_activation_on_parent) {
  return surface_manager_->CreateSurface(
      weak_factory_.GetWeakPtr(), surface_info,
      frame_sink_manager_->GetPrimaryBeginFrameSource(), needs_sync_tokens_,
      block_activation_on_parent);
}

SubmitResult CompositorFrameSinkSupport::MaybeSubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    base::Optional<HitTestRegionList> hit_test_region_list,
    uint64_t submit_time,
    mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback callback) {
  SubmitResult result = MaybeSubmitCompositorFrameInternal(
      local_surface_id, std::move(frame), std::move(hit_test_region_list),
      submit_time, std::move(callback));
  UMA_HISTOGRAM_ENUMERATION(
      "Compositing.CompositorFrameSinkSupport.SubmitResult", result);
  return result;
}

void CompositorFrameSinkSupport::AttachCaptureClient(
    CapturableFrameSink::Client* client) {
  DCHECK(!base::ContainsValue(capture_clients_, client));
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
    const LocalSurfaceId& local_surface_id,
    std::unique_ptr<CopyOutputRequest> copy_request) {
  copy_output_requests_.push_back(
      std::make_pair(local_surface_id, std::move(copy_request)));
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
  return &surface->GetActiveFrame().metadata;
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
    case SubmitResult::SURFACE_INVARIANTS_VIOLATION:
      return "Surface invariants violation";
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

}  // namespace viz
