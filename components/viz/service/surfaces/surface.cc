// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/surfaces/referenced_surface_tracker.h"
#include "components/viz/service/surfaces/surface_allocation_group.h"
#include "components/viz/service/surfaces/surface_client.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"

namespace viz {

namespace {

// Adds the given |request| to the requests of the given |render_pass|, removing
// any duplicate requests made by the same source.
void RequestCopyOfOutputOnRenderPass(std::unique_ptr<CopyOutputRequest> request,
                                     CompositorRenderPass& render_pass) {
  if (request->has_source()) {
    const base::UnguessableToken& source = request->source();
    // Remove existing CopyOutputRequests made on the Surface by the same
    // source.
    std::erase_if(render_pass.copy_requests,
                  [&source](const std::unique_ptr<CopyOutputRequest>& x) {
                    return x->has_source() && x->source() == source;
                  });
  }
  render_pass.copy_requests.push_back(std::move(request));
}

bool ShouldBlockActivationOnDependenciesWhenInteractive() {
  return !features::ShouldDrawImmediatelyWhenInteractive();
}

}  // namespace

Surface::PresentationHelper::PresentationHelper(
    base::WeakPtr<SurfaceClient> surface_client,
    uint32_t frame_token)
    : surface_client_(std::move(surface_client)), frame_token_(frame_token) {}

Surface::PresentationHelper::~PresentationHelper() {
  // The class that called TakePresentationHelperForPresentNotification
  // should have called present on this helper. If not, give a Failure feedback
  // to the appropriate surface client.
  DidPresent(base::TimeTicks(), gfx::SwapTimings(),
             gfx::PresentationFeedback::Failure());
}

void Surface::PresentationHelper::DidPresent(
    base::TimeTicks draw_start_timestamp,
    const gfx::SwapTimings& swap_timings,
    const gfx::PresentationFeedback& feedback) {
  if (surface_client_) {
    surface_client_->OnSurfacePresented(frame_token_, draw_start_timestamp,
                                        swap_timings, feedback);
  }

  surface_client_ = nullptr;
}

Surface::Surface(const SurfaceInfo& surface_info,
                 SurfaceManager* surface_manager,
                 SurfaceAllocationGroup* allocation_group,
                 base::WeakPtr<SurfaceClient> surface_client,
                 const SurfaceId& pending_copy_surface_id,
                 size_t max_uncommitted_frames)
    : surface_info_(surface_info),
      surface_manager_(surface_manager),
      surface_client_(std::move(surface_client)),
      pending_copy_surface_id_(pending_copy_surface_id),
      allocation_group_(allocation_group),
      max_uncommitted_frames_(max_uncommitted_frames) {
  TRACE_EVENT_ASYNC_BEGIN1(TRACE_DISABLED_BY_DEFAULT("viz.surface_lifetime"),
                           "Surface", this, "surface_info",
                           surface_info.ToString());
  allocation_group_->RegisterSurface(this);
  is_fallback_ =
      allocation_group_->GetLastReference().IsNewerThan(surface_id());
}

Surface::~Surface() {
  ClearCopyRequests();

  surface_manager_->SurfaceDestroyed(this);

  for (auto& frame : uncommitted_frames_) {
    UnrefFrameResourcesAndRunCallbacks(std::move(frame));
  }

  UnrefFrameResourcesAndRunCallbacks(std::move(pending_frame_data_));
  UnrefFrameResourcesAndRunCallbacks(std::move(active_frame_data_));

  // Unregister this surface as the embedder of all the allocation groups that
  // it references.
  for (SurfaceAllocationGroup* group : referenced_allocation_groups_)
    group->UnregisterActiveEmbedder(this);
  for (SurfaceAllocationGroup* group : blocking_allocation_groups_)
    group->UnregisterBlockedEmbedder(this, false /* did_activate */);

  DCHECK(deadline_);
  deadline_->Cancel();

  TRACE_EVENT_ASYNC_END1(TRACE_DISABLED_BY_DEFAULT("viz.surface_lifetime"),
                         "Surface", this, "surface_info",
                         surface_info_.ToString());
  allocation_group_->UnregisterSurface(this);
  if (surface_client_) {
    surface_client_->OnSurfaceDestroyed(this);
  }
}

void Surface::SetDependencyDeadline(
    std::unique_ptr<SurfaceDependencyDeadline> deadline) {
  deadline_ = std::move(deadline);
}

void Surface::SetPreviousFrameSurface(Surface* surface) {
  DCHECK(surface && (HasActiveFrame() || HasPendingFrame()));
  previous_frame_surface_id_ = surface->surface_id();
}

void Surface::UpdateSurfaceReferences() {
  const base::flat_set<SurfaceId>& existing_referenced_surfaces =
      surface_manager_->GetSurfacesReferencedByParent(surface_id());

  // Populate list of surface references to add and remove by getting the
  // difference between existing surface references and surface references for
  // latest activated CompositorFrame.
  std::vector<SurfaceReference> references_to_add;
  std::vector<SurfaceReference> references_to_remove;
  GetSurfaceReferenceDifference(surface_id(), existing_referenced_surfaces,
                                active_referenced_surfaces(),
                                &references_to_add, &references_to_remove);

  // Modify surface references stored in SurfaceManager.
  if (!references_to_add.empty())
    surface_manager_->AddSurfaceReferences(references_to_add);
  if (!references_to_remove.empty())
    surface_manager_->RemoveSurfaceReferences(references_to_remove);
}

void Surface::OnChildActivatedForActiveFrame(const SurfaceId& activated_id) {
  DCHECK(HasActiveFrame());

  for (auto& surface_range : GetActiveFrame().metadata.referenced_surfaces) {
    if (surface_range.IsInRangeInclusive(activated_id)) {
      // If |activated_id| is included in any of the surface reference then
      // recompute the active surface references. This must handle the case
      // where a SurfaceId is included in multiple surface ranges.
      RecomputeActiveReferencedSurfaces();
      return;
    }
  }
}

void Surface::SetIsFallbackAndMaybeActivate() {
  is_fallback_ = true;
  if (HasPendingFrame())
    ActivatePendingFrameForDeadline();
}

void Surface::ActivateIfDeadlinePassed() {
  DCHECK(HasPendingFrame());
  if (!deadline_->HasDeadlinePassed())
    return;
  TRACE_EVENT1("viz", "Surface deadline passed", "FrameSinkId",
               surface_id().frame_sink_id().ToString());
  ActivatePendingFrameForDeadline();
}

Surface::QueueFrameResult Surface::QueueFrame(
    CompositorFrame frame,
    uint64_t frame_index,
    base::ScopedClosureRunner frame_rejected_callback) {
  if (frame.size_in_pixels() != surface_info_.size_in_pixels() ||
      frame.device_scale_factor() != surface_info_.device_scale_factor()) {
    TRACE_EVENT_INSTANT0("viz", "Surface invariants violation",
                         TRACE_EVENT_SCOPE_THREAD);
    return QueueFrameResult::REJECTED;
  }

  // Receive and track the resources referenced from the CompositorFrame
  // regardless of whether it's pending or active.
  surface_client_->ReceiveFromChild(frame.resource_list);

  QueueFrameResult result = QueueFrameResult::ACCEPTED_PENDING;

  if (!max_uncommitted_frames_) {
    result = CommitFrame(FrameData(std::move(frame), frame_index));
  } else {
    // Return oldest frame if uncommitted queue is full.
    DCHECK_LE(uncommitted_frames_.size(), max_uncommitted_frames_);
    if (uncommitted_frames_.size() == max_uncommitted_frames_) {
      TRACE_EVENT_INSTANT1("viz", "DropUncommitedFrame",
                           TRACE_EVENT_SCOPE_THREAD, "queue_length",
                           uncommitted_frames_.size());

      UnrefFrameResourcesAndRunCallbacks(
          std::move(uncommitted_frames_.front()));
      uncommitted_frames_.pop_front();
    }

    uncommitted_frames_.push_back(FrameData(std::move(frame), frame_index));

    // If we still have space in queue we should send ack the client because we
    // can receive another frame without dropping it.
    if (uncommitted_frames_.size() < max_uncommitted_frames_) {
      TRACE_EVENT_INSTANT1("viz", "AckingUncommitedFrame",
                           TRACE_EVENT_SCOPE_THREAD, "queue_length",
                           uncommitted_frames_.size());
      uncommitted_frames_.back().SendAckIfNeeded(surface_client_.get());
    }

    surface_manager_->OnSurfaceHasNewUncommittedFrame(this);
  }
  // The frame should not fail to display beyond this point. Release the
  // callback so it is not called.
  std::ignore = frame_rejected_callback.Release();

  return result;
}

Surface::QueueFrameResult Surface::CommitFrame(FrameData frame) {
  TRACE_EVENT1("viz", "Surface::CommitFrame", "SurfaceId",
               surface_id().ToString());

  is_latency_info_taken_ = false;

  if (active_frame_data_ || pending_frame_data_)
    previous_frame_surface_id_ = surface_id();

  TakePendingLatencyInfo(&frame.frame.metadata.latency_info);

  std::optional<FrameData> previous_pending_frame_data =
      std::move(pending_frame_data_);
  pending_frame_data_.reset();

  UpdateActivationDependencies(frame.frame);

  QueueFrameResult result = QueueFrameResult::ACCEPTED_ACTIVE;
  if (activation_dependencies_.empty()) {
    // If there are no blockers, then immediately activate the frame.
    ActivateFrame(std::move(frame));
  } else {
    pending_frame_data_ = std::move(frame);

    auto traced_value = std::make_unique<base::trace_event::TracedValue>();
    traced_value->BeginArray("Pending");
    for (auto& it : activation_dependencies_)
      traced_value->AppendString(it.ToString());
    traced_value->EndArray();
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
        "viz", "SurfaceQueuedPending", TRACE_ID_LOCAL(this), "LocalSurfaceId",
        surface_info_.id().ToString(), "ActivationDependencies",
        std::move(traced_value));

    deadline_->Set(ResolveFrameDeadline(pending_frame_data_->frame));
    if (deadline_->HasDeadlinePassed()) {
      ActivatePendingFrameForDeadline();
    } else {
      // If we are blocked on another Surface, and its latest frame is unacked,
      // we send the Ack now. This will allow frame production to continue for
      // that client, leading to the group being unblocked.
      for (SurfaceAllocationGroup* it : blocking_allocation_groups_) {
        it->AckLastestActiveUnAckedFrame();
      }
      result = QueueFrameResult::ACCEPTED_PENDING;
    }
  }

  // Returns resources for the previous pending frame.
  UnrefFrameResourcesAndRunCallbacks(std::move(previous_pending_frame_data));

  if (surface_client_)
    surface_client_->OnSurfaceCommitted(this);

  return result;
}

void Surface::RequestCopyOfOutput(
    PendingCopyOutputRequest pending_copy_output_request) {
  TRACE_EVENT1("viz", "Surface::RequestCopyOfOutput", "has_active_frame_data",
               !!active_frame_data_);

  if (!pending_copy_output_request.subtree_capture_id.is_valid()) {
    RequestCopyOfOutputOnRootRenderPass(
        std::move(pending_copy_output_request.copy_output_request));
    return;
  }

  if (!active_frame_data_)
    return;

  for (auto& render_pass : GetActiveFrame().render_pass_list) {
    if (render_pass->subtree_capture_id ==
        pending_copy_output_request.subtree_capture_id) {
      RequestCopyOfOutputOnRenderPass(
          std::move(pending_copy_output_request.copy_output_request),
          *render_pass);
      return;
    }
  }
}

void Surface::RequestCopyOfOutputOnRootRenderPass(
    std::unique_ptr<CopyOutputRequest> copy_request) {
  TRACE_EVENT1("viz", "Surface::RequestCopyOfOutputOnRootRenderPass",
               "has_active_frame_data", !!active_frame_data_);
  if (!active_frame_data_)
    return;  // |copy_request| auto-sends empty result on out-of-scope.

  RequestCopyOfOutputOnRenderPass(std::move(copy_request),
                                  *GetActiveFrame().render_pass_list.back());
}

bool Surface::RequestCopyOfOutputOnActiveFrameRenderPassId(
    std::unique_ptr<CopyOutputRequest> copy_request,
    CompositorRenderPassId render_pass_id) {
  TRACE_EVENT1("viz", "Surface::RequestCopyOfOurpurRenderPassId",
               "has_active_frame_data", !!active_frame_data_);
  if (!active_frame_data_)
    return false;

  // Find a render pass with a given id, and attach the copy output request on
  // it.
  for (auto& render_pass : GetActiveFrame().render_pass_list) {
    if (render_pass->id == render_pass_id) {
      RequestCopyOfOutputOnRenderPass(std::move(copy_request), *render_pass);
      return true;
    }
  }
  return false;
}

void Surface::OnActivationDependencyResolved(
    const SurfaceId& activation_dependency,
    SurfaceAllocationGroup* group) {
  DCHECK(activation_dependencies_.count(activation_dependency));
  activation_dependencies_.erase(activation_dependency);
  blocking_allocation_groups_.erase(group);
  if (!activation_dependencies_.empty())
    return;
  // All blockers have been cleared. The surface can be activated now.
  ActivatePendingFrame();
}

void Surface::ActivatePendingFrameForDeadline() {
  if (!pending_frame_data_)
    return;

  if (!activation_dependencies_.empty()) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("viz", "SurfaceQueuedPending",
                                    TRACE_ID_LOCAL(this));
  }

  // If a frame is being activated because of a deadline, then clear its set
  // of blockers.
  activation_dependencies_.clear();

  ActivatePendingFrame();
}

Surface::FrameData::FrameData(CompositorFrame&& frame, uint64_t frame_index)
    : frame(std::move(frame)), frame_index(frame_index) {}

Surface::FrameData::FrameData(FrameData&& other) = default;

Surface::FrameData& Surface::FrameData::operator=(FrameData&& other) = default;

Surface::FrameData::~FrameData() = default;

void Surface::FrameData::SendAckIfNeeded(SurfaceClient* client) {
  if (!frame_acked) {
    frame_acked = true;
    if (client)
      client->SendCompositorFrameAck();
  }
}

void Surface::ActivatePendingFrame() {
  DCHECK(pending_frame_data_);
  FrameData frame_data = std::move(*pending_frame_data_);
  pending_frame_data_.reset();

  std::optional<base::TimeDelta> duration = deadline_->Cancel();
  if (duration.has_value()) {
    TRACE_EVENT_INSTANT2("viz", "SurfaceSynchronizationEvent",
                         TRACE_EVENT_SCOPE_THREAD, "surface_id",
                         surface_info_.id().ToString(), "duration_ms",
                         duration.value().InMilliseconds());
  }

  ActivateFrame(std::move(frame_data));
}

void Surface::CommitFramesRecursively(const CommitPredicate& predicate) {
  TRACE_EVENT1("viz", "Surface::CommitFramesRecursively", "SurfaceId",
               surface_id().ToString());

  // This should only be called if we use uncommitted frames queue.
  DCHECK_GT(max_uncommitted_frames_, 0u);

  while (!uncommitted_frames_.empty()) {
    const auto& ack =
        uncommitted_frames_.front().frame.metadata.begin_frame_ack;

    if (!predicate(surface_id(), ack.frame_id)) {
      break;
    }

    CommitFrame(std::move(uncommitted_frames_.front()));
    uncommitted_frames_.pop_front();
  }

  if (HasPendingFrame()) {
    for (auto& range : pending_frame_data_->frame.metadata.referenced_surfaces)
      surface_manager_->CommitFramesInRangeRecursively(range, predicate);
  }

  if (HasActiveFrame()) {
    for (auto& range : active_frame_data_->frame.metadata.referenced_surfaces)
      surface_manager_->CommitFramesInRangeRecursively(range, predicate);
  }

  // If we freed up some space in queue send ack for the last frame if it's
  // still unacked, so client can continue producing frames.
  if (uncommitted_frames_.size() < max_uncommitted_frames_) {
    if (!uncommitted_frames_.empty())
      uncommitted_frames_.back().SendAckIfNeeded(surface_client_.get());

      // Only last frame can be unacked because we ack frames as we put them in
      // queue if queue isn't full. If we acked frame above, now verify that
      // they all are acked, to ensure we ack frame in order.
#if DCHECK_IS_ON()
    for (auto& frames : uncommitted_frames_) {
      DCHECK(frames.frame_acked);
    }
#endif
  }
}

std::optional<uint64_t> Surface::GetFirstUncommitedFrameIndex() {
  if (uncommitted_frames_.empty())
    return std::nullopt;
  return uncommitted_frames_.front().frame_index;
}

std::optional<uint64_t> Surface::GetUncommitedFrameIndexNewerThan(
    uint64_t frame_index) {
  for (auto& frame : uncommitted_frames_) {
    if (frame.frame_index > frame_index) {
      return frame.frame_index;
    }
  }
  return std::nullopt;
}

void Surface::ResetPendingCopySurfaceId() {
  CHECK(pending_copy_surface_id_.is_valid());
  pending_copy_surface_id_ = SurfaceId();
  // It's an error to compute the surface references if the current surface does
  // not have an active frame.
  if (HasActiveFrame()) {
    RecomputeActiveReferencedSurfaces();
  }
}

void Surface::UpdateReferencedAllocationGroups(
    std::vector<SurfaceAllocationGroup*> new_referenced_allocation_groups) {
  base::flat_set<raw_ptr<SurfaceAllocationGroup, CtnExperimental>> new_set(
      new_referenced_allocation_groups.begin(),
      new_referenced_allocation_groups.end());

  for (SurfaceAllocationGroup* group : referenced_allocation_groups_) {
    if (!new_set.count(group))
      group->UnregisterActiveEmbedder(this);
  }

  for (SurfaceAllocationGroup* group : new_set) {
    if (!referenced_allocation_groups_.count(group))
      group->RegisterActiveEmbedder(this);
  }

  referenced_allocation_groups_ = std::move(new_set);
}

void Surface::RecomputeActiveReferencedSurfaces() {
  // Extract the latest in flight surface from the ranges in the frame then
  // notify SurfaceManager of the new references.
  active_referenced_surfaces_.clear();
  std::vector<SurfaceAllocationGroup*> new_referenced_allocation_groups;
  for (const SurfaceRange& surface_range :
       active_frame_data_->frame.metadata.referenced_surfaces) {
    // Figure out what surface in the |surface_range| needs to be referenced.
    Surface* surface =
        surface_manager_->GetLatestInFlightSurface(surface_range);
    if (surface) {
      active_referenced_surfaces_.insert(surface->surface_id());
    }
    // The allocation group for the end of the SurfaceRange should always be
    // referenced.
    SurfaceAllocationGroup* end_allocation_group =
        surface_manager_->GetOrCreateAllocationGroupForSurfaceId(
            surface_range.end());
    if (end_allocation_group) {
      new_referenced_allocation_groups.push_back(end_allocation_group);
      end_allocation_group->UpdateLastActiveReferenceAndMaybeActivate(
          surface_range.end());
    }
    // Only reference the allocation group for the start of SurfaceRange if the
    // current referenced surface is a part of it.
    if (surface_range.HasDifferentEmbedTokens() &&
        (!surface ||
         surface->surface_id().HasSameEmbedTokenAs(*surface_range.start()))) {
      SurfaceAllocationGroup* start_allocation_group =
          surface_manager_->GetOrCreateAllocationGroupForSurfaceId(
              *surface_range.start());
      if (start_allocation_group) {
        new_referenced_allocation_groups.push_back(start_allocation_group);
        start_allocation_group->UpdateLastActiveReferenceAndMaybeActivate(
            *surface_range.start());
      }
    }
  }

  // Makes sure `pending_copy_surface_id_` is reachable from `this` during
  // aggregation.
  if (pending_copy_surface_id_.is_valid()) {
    active_referenced_surfaces_.insert(pending_copy_surface_id_);
  }

  UpdateReferencedAllocationGroups(std::move(new_referenced_allocation_groups));
  UpdateSurfaceReferences();
}

// A frame is activated if all its Surface ID dependencies are active or a
// deadline has hit and the frame was forcibly activated.
void Surface::ActivateFrame(FrameData frame_data) {
  TRACE_EVENT1("viz", "Surface::ActivateFrame", "SurfaceId",
               surface_id().ToString());

  // Save root pass copy requests.
  std::vector<std::unique_ptr<CopyOutputRequest>> old_copy_requests;
  if (active_frame_data_) {
    std::swap(old_copy_requests,
              active_frame_data_->frame.render_pass_list.back()->copy_requests);
  }

  ClearCopyRequests();

  TakeActiveLatencyInfo(&frame_data.frame.metadata.latency_info);

  std::optional<FrameData> previous_frame_data = std::move(active_frame_data_);

  active_frame_data_ = std::move(frame_data);

  // We no longer have a pending frame, so unregister self from
  // |blocking_allocation_groups_|.
  for (SurfaceAllocationGroup* group : blocking_allocation_groups_)
    group->UnregisterBlockedEmbedder(this, true /* did_activate */);
  blocking_allocation_groups_.clear();

  RecomputeActiveReferencedSurfaces();

  for (auto& copy_request : old_copy_requests)
    RequestCopyOfOutputOnRootRenderPass(std::move(copy_request));

  UnrefFrameResourcesAndRunCallbacks(std::move(previous_frame_data));

  // This should happen before calling SurfaceManager::FirstSurfaceActivation(),
  // as that notifies observers which may have side effects for
  // |surface_client_|. See https://crbug.com/821855.
  if (surface_client_)
    surface_client_->OnSurfaceActivated(this);

  if (!seen_first_frame_activation_) {
    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "LocalSurfaceId.Submission.Flow",
        TRACE_ID_GLOBAL(
            surface_info_.id().local_surface_id().submission_trace_id()),
        TRACE_EVENT_FLAG_FLOW_IN, "step", "FirstSurfaceActivation",
        "surface_id", surface_info_.id().ToString());

    seen_first_frame_activation_ = true;
    allocation_group_->OnFirstSurfaceActivation(this);
    surface_manager_->FirstSurfaceActivation(surface_info_);
  }

  surface_manager_->SurfaceActivated(this);

  // Defer notifying the embedder of an updated token until the frame has been
  // completely processed.
  const auto& metadata = GetActiveFrameMetadata();
  if (surface_client_ && metadata.send_frame_token_to_embedder)
    surface_client_->OnFrameTokenChanged(metadata.frame_token);
}

FrameDeadline Surface::ResolveFrameDeadline(
    const CompositorFrame& current_frame) {
  // Fallback surfaces should activate immediately so that the client receives
  // the ack and can submit a frame to the primary surface.
  if (is_fallback_)
    return FrameDeadline::MakeZero();

  // If there is an embedder of this surface that has already activated, that
  // means the embedder doesn't wish to block on this surface, i.e. either it
  // had a zero deadline or its deadline has already passed. If we don't have an
  // active frame already, active this frame immediately so we have something to
  // show.
  if (!HasActiveFrame() &&
      allocation_group_->GetLastActiveReference() == surface_id()) {
    return FrameDeadline::MakeZero();
  }

  const std::optional<uint32_t>& default_deadline =
      surface_manager_->activation_deadline_in_frames();
  const FrameDeadline& deadline = current_frame.metadata.deadline;
  uint32_t deadline_in_frames = deadline.deadline_in_frames();

  // If no default deadline is available then all deadlines are treated as
  // effectively infinite deadlines.
  if (!default_deadline || deadline.use_default_lower_bound_deadline()) {
    deadline_in_frames = std::max(
        deadline_in_frames,
        default_deadline.value_or(std::numeric_limits<uint32_t>::max()));
  }

  return FrameDeadline(deadline.frame_start_time(), deadline_in_frames,
                       deadline.frame_interval(),
                       false /* use_default_lower_bound_deadline */);
}

void Surface::UpdateActivationDependencies(
    const CompositorFrame& current_frame) {
  for (SurfaceAllocationGroup* group : blocking_allocation_groups_)
    group->UnregisterBlockedEmbedder(this, false /* did_activate */);
  blocking_allocation_groups_.clear();
  activation_dependencies_.clear();

  // If the client has specified a deadline of zero, there is no need to figure
  // out the activation dependencies since the frame will activate immediately.
  if (current_frame.metadata.deadline.IsZero())
    return;

  bool should_block_on_dependencies =
      ShouldBlockActivationOnDependenciesWhenInteractive() ||
      !current_frame.metadata.is_handling_interaction;

  if (!should_block_on_dependencies) {
    return;
  }

  base::flat_set<raw_ptr<SurfaceAllocationGroup, CtnExperimental>>
      new_blocking_allocation_groups;
  std::vector<SurfaceId> new_activation_dependencies;
  for (const SurfaceId& surface_id :
       current_frame.metadata.activation_dependencies) {
    SurfaceAllocationGroup* group =
        surface_manager_->GetOrCreateAllocationGroupForSurfaceId(surface_id);
    if (base::Contains(new_blocking_allocation_groups, group))
      continue;
    if (group)
      group->UpdateLastPendingReferenceAndMaybeActivate(surface_id);
    Surface* dependency = surface_manager_->GetSurfaceForId(surface_id);
    if (dependency && dependency->HasActiveFrame()) {
      // Normally every creation of SurfaceAllocationGroup should be followed by
      // a call to Register* to keep it alive. However, since this one already
      // has a registered surface, we don't have to do that.
      DCHECK(!group->IsReadyToDestroy());
      continue;
    }
    if (group) {
      group->RegisterBlockedEmbedder(this, surface_id);
      new_blocking_allocation_groups.insert(group);
    }
    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "LocalSurfaceId.Embed.Flow",
        TRACE_ID_GLOBAL(surface_id.local_surface_id().embed_trace_id()),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
        "AddedActivationDependency", "child_surface_id", surface_id.ToString());
    new_activation_dependencies.push_back(surface_id);
  }
  activation_dependencies_ = std::move(new_activation_dependencies);
  blocking_allocation_groups_ = std::move(new_blocking_allocation_groups);
}

void Surface::TakeCopyOutputRequests(Surface::CopyRequestsMap* copy_requests) {
  DCHECK(copy_requests->empty());
  if (!active_frame_data_)
    return;

  for (const auto& render_pass : GetActiveFrame().render_pass_list) {
    for (auto& request : render_pass->copy_requests) {
      copy_requests->insert(
          std::make_pair(render_pass->id, std::move(request)));
    }
    render_pass->copy_requests.clear();
  }
  MarkAsDrawn();
}

void Surface::TakeCopyOutputRequestsFromClient() {
  if (!surface_client_)
    return;
  for (PendingCopyOutputRequest& request_params :
       surface_client_->TakeCopyOutputRequests(
           surface_id().local_surface_id())) {
    RequestCopyOfOutput(std::move(request_params));
  }
}

bool Surface::HasCopyOutputRequests() const {
  return active_frame_data_ && GetActiveFrame().HasCopyOutputRequests();
}

const CompositorFrame& Surface::GetActiveFrame() const {
  DCHECK(active_frame_data_);
  return active_frame_data_->frame;
}

const CompositorFrameMetadata& Surface::GetActiveFrameMetadata() const {
  DCHECK(active_frame_data_);
  return active_frame_data_->frame.metadata;
}

const FrameIntervalInputs& Surface::GetFrameIntervalInputs() const {
  DCHECK(active_frame_data_);
  return active_frame_data_->frame.metadata.frame_interval_inputs;
}

void Surface::SetActiveFrameForViewTransition(CompositorFrame frame) {
  CHECK(active_frame_data_.has_value());

  active_frame_data_->frame = std::move(frame);
}

const CompositorFrame& Surface::GetPendingFrame() {
  DCHECK(pending_frame_data_);
  return pending_frame_data_->frame;
}

void Surface::TakeActiveLatencyInfo(
    std::vector<ui::LatencyInfo>* latency_info) {
  if (!active_frame_data_)
    return;
  TakeLatencyInfoFromFrame(&active_frame_data_->frame, latency_info);
}

void Surface::TakeActiveAndPendingLatencyInfo(
    std::vector<ui::LatencyInfo>* latency_info) {
  TakeActiveLatencyInfo(latency_info);
  TakePendingLatencyInfo(latency_info);
  is_latency_info_taken_ = true;
}

std::unique_ptr<Surface::PresentationHelper>
Surface::TakePresentationHelperForPresentNotification() {
  if (active_frame_data_ &&
      !active_frame_data_->will_be_notified_of_presentation) {
    active_frame_data_->will_be_notified_of_presentation = true;
    return std::make_unique<PresentationHelper>(
        client(), active_frame_data_->frame.metadata.frame_token);
  }
  return nullptr;
}

void Surface::SendAckToClient() {
  if (active_frame_data_)
    active_frame_data_->SendAckIfNeeded(surface_client_.get());
}

void Surface::MarkAsDrawn() {
  if (!active_frame_data_)
    return;
  active_frame_data_->frame_drawn = true;
  if (surface_client_)
    surface_client_->OnSurfaceWillDraw(this);
}

void Surface::NotifyAggregatedDamage(const gfx::Rect& damage_rect,
                                     base::TimeTicks expected_display_time) {
  if (!active_frame_data_ || !surface_client_)
    return;
  surface_client_->OnSurfaceAggregatedDamage(
      this, surface_id().local_surface_id(), active_frame_data_->frame,
      damage_rect, expected_display_time);
}

bool Surface::IsVideoCaptureOnFromClient() {
  if (!surface_client_)
    return false;

  return surface_client_->IsVideoCaptureStarted();
}

base::flat_set<base::PlatformThreadId> Surface::GetThreadIds() {
  if (!surface_client_)
    return {};

  return surface_client_->GetThreadIds();
}

void Surface::UnrefFrameResourcesAndRunCallbacks(
    std::optional<FrameData> frame_data) {
  if (!frame_data || !surface_client_)
    return;

  std::vector<ReturnedResource> resources =
      TransferableResource::ReturnResources(frame_data->frame.resource_list);
  // No point in returning same sync token to sender.
  for (auto& resource : resources)
    resource.sync_token.Clear();
  surface_client_->UnrefResources(std::move(resources));

  frame_data->SendAckIfNeeded(surface_client_.get());

  // If we won't be getting a presented notification, we'll notify the client
  // when the frame is unref'd.
  if (!frame_data->will_be_notified_of_presentation && surface_client_) {
    surface_client_->OnSurfacePresented(frame_data->frame.metadata.frame_token,
                                        base::TimeTicks(), gfx::SwapTimings(),
                                        gfx::PresentationFeedback::Failure());
  }

  // Usually the LatencyInfo was already taken during aggregation or when the
  // surface was replaced. If neither happened, terminate the LatencyInfo now.
  for (ui::LatencyInfo& info : frame_data->frame.metadata.latency_info)
    info.Terminate();
}

void Surface::ClearCopyRequests() {
  if (active_frame_data_) {
    for (const auto& render_pass : GetActiveFrame().render_pass_list) {
      // When the container is cleared, all copy requests within it will
      // auto-send an empty result as they are being destroyed.
      render_pass->copy_requests.clear();
    }
  }
}

void Surface::TakePendingLatencyInfo(
    std::vector<ui::LatencyInfo>* latency_info) {
  if (!pending_frame_data_)
    return;
  TakeLatencyInfoFromFrame(&pending_frame_data_->frame, latency_info);
}

// static
void Surface::TakeLatencyInfoFromFrame(
    CompositorFrame* frame,
    std::vector<ui::LatencyInfo>* latency_info) {
  if (latency_info->empty()) {
    frame->metadata.latency_info.swap(*latency_info);
    return;
  }
  base::ranges::copy(frame->metadata.latency_info,
                     std::back_inserter(*latency_info));
  frame->metadata.latency_info.clear();
  if (!ui::LatencyInfo::Verify(*latency_info,
                               "Surface::TakeLatencyInfoFromFrame")) {
    for (auto& info : *latency_info) {
      info.Terminate();
    }
    latency_info->clear();
  }
}

void Surface::OnWillBeDrawn() {
  if (!seen_first_surface_embedding_) {
    seen_first_surface_embedding_ = true;

    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "LocalSurfaceId.Embed.Flow",
        TRACE_ID_GLOBAL(surface_info_.id().local_surface_id().embed_trace_id()),
        TRACE_EVENT_FLAG_FLOW_IN, "step", "FirstSurfaceEmbedding", "surface_id",
        surface_info_.id().ToString());
  }
  surface_manager_->SurfaceWillBeDrawn(this);
  MarkAsDrawn();
}

void Surface::ActivatePendingFrameForInheritedDeadline() {
  // Deadline inheritance implies that this surface was blocking the embedder,
  // so there shouldn't be an active frame.
  DCHECK(!HasActiveFrame());
  ActivatePendingFrameForDeadline();
}

std::unique_ptr<gfx::DelegatedInkMetadata> Surface::TakeDelegatedInkMetadata() {
  DCHECK(active_frame_data_);
  return active_frame_data_->TakeDelegatedInkMetadata();
}

}  // namespace viz
