// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
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
  if (surface_client_ && frame_token_) {
    surface_client_->OnSurfacePresented(frame_token_, draw_start_timestamp,
                                        swap_timings, feedback);
  }

  surface_client_ = nullptr;
}

Surface::Surface(const SurfaceInfo& surface_info,
                 SurfaceManager* surface_manager,
                 SurfaceAllocationGroup* allocation_group,
                 base::WeakPtr<SurfaceClient> surface_client)
    : surface_info_(surface_info),
      surface_manager_(surface_manager),
      surface_client_(std::move(surface_client)),
      allocation_group_(allocation_group) {
  TRACE_EVENT_ASYNC_BEGIN1(TRACE_DISABLED_BY_DEFAULT("viz.surface_lifetime"),
                           "Surface", this, "surface_info",
                           surface_info.ToString());
  allocation_group_->RegisterSurface(this);
  is_fallback_ =
      allocation_group_->GetLastReference().IsNewerThan(surface_id());
}

Surface::~Surface() {
  ClearCopyRequests();

  if (surface_client_)
    surface_client_->OnSurfaceDestroyed(this);
  surface_manager_->SurfaceDestroyed(this);

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
}

void Surface::SetDependencyDeadline(
    std::unique_ptr<SurfaceDependencyDeadline> deadline) {
  deadline_ = std::move(deadline);
}

void Surface::SetPreviousFrameSurface(Surface* surface) {
  DCHECK(surface && (HasActiveFrame() || HasPendingFrame()));
  previous_frame_surface_id_ = surface->surface_id();
}

void Surface::RefResources(const std::vector<TransferableResource>& resources) {
  if (surface_client_)
    surface_client_->RefResources(resources);
}

void Surface::UnrefResources(const std::vector<ReturnedResource>& resources) {
  if (surface_client_)
    surface_client_->UnrefResources(resources);
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

  for (size_t i = 0;
       i < active_frame_data_->frame.metadata.referenced_surfaces.size(); i++) {
    const SurfaceRange& surface_range =
        active_frame_data_->frame.metadata.referenced_surfaces[i];
    if (!surface_range.IsInRangeInclusive(activated_id))
      continue;

    const SurfaceId& last_id = last_surface_id_for_range_[i];
    // If we already have a reference to a surface in the primary's allocation
    // group, we should already be unregistered from the allocation group of the
    // fallback so we shouldn't receive SurfaceIds from that group.
    DCHECK(!surface_range.HasDifferentEmbedTokens() || !last_id.is_valid() ||
           !last_id.HasSameEmbedTokenAs(surface_range.end()) ||
           activated_id.HasSameEmbedTokenAs(last_id));

    // Remove the old reference.
    if (last_id.is_valid()) {
      auto old_it = active_referenced_surfaces_.find(last_id);
      if (old_it != active_referenced_surfaces_.end())
        active_referenced_surfaces_.erase(old_it);
      surface_manager_->RemoveSurfaceReferences(
          {SurfaceReference(surface_info_.id(), last_id)});
    }

    // Add a new reference.
    active_referenced_surfaces_.insert(activated_id);
    surface_manager_->AddSurfaceReferences(
        {SurfaceReference(surface_info_.id(), activated_id)});

    // If we were referencing a surface in the allocation group of the
    // fallback, but now there is a surface available in the allocation group
    // of the primary, unregister this surface from the allocation group of
    // the fallback.
    if (activated_id.HasSameEmbedTokenAs(surface_range.end()) &&
        surface_range.HasDifferentEmbedTokens() &&
        (!last_id.is_valid() || !last_id.HasSameEmbedTokenAs(activated_id))) {
      DCHECK(surface_range.start());
      DCHECK(!last_id.is_valid() ||
             last_id.HasSameEmbedTokenAs(*surface_range.start()));
      SurfaceAllocationGroup* group =
          surface_manager_->GetAllocationGroupForSurfaceId(
              *surface_range.start());
      if (group && referenced_allocation_groups_.count(group)) {
        group->UnregisterActiveEmbedder(this);
        referenced_allocation_groups_.erase(group);
      }
    }

    // Update the referenced surface for this range.
    last_surface_id_for_range_[i] = activated_id;
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

  QueueFrameResult result = QueueFrameResult::ACCEPTED_ACTIVE;

  is_latency_info_taken_ = false;

  if (active_frame_data_ || pending_frame_data_)
    previous_frame_surface_id_ = surface_id();

  TakePendingLatencyInfo(&frame.metadata.latency_info);

  base::Optional<FrameData> previous_pending_frame_data =
      std::move(pending_frame_data_);
  pending_frame_data_.reset();

  UpdateActivationDependencies(frame);

  // Receive and track the resources referenced from the CompositorFrame
  // regardless of whether it's pending or active.
  surface_client_->ReceiveFromChild(frame.resource_list);

  if (activation_dependencies_.empty()) {
    // If there are no blockers, then immediately activate the frame.
    ActivateFrame(FrameData(std::move(frame), frame_index), base::nullopt);
  } else {
    pending_frame_data_ = FrameData(std::move(frame), frame_index);

    deadline_->Set(ResolveFrameDeadline(pending_frame_data_->frame));
    if (deadline_->HasDeadlinePassed()) {
      ActivatePendingFrameForDeadline();
    } else {
      result = QueueFrameResult::ACCEPTED_PENDING;
    }
  }

  // Returns resources for the previous pending frame.
  UnrefFrameResourcesAndRunCallbacks(std::move(previous_pending_frame_data));

  // The frame should not fail to display beyond this point. Release the
  // callback so it is not called.
  (void)frame_rejected_callback.Release();

  return result;
}

void Surface::RequestCopyOfOutput(
    std::unique_ptr<CopyOutputRequest> copy_request) {
  if (!active_frame_data_)
    return;  // |copy_request| auto-sends empty result on out-of-scope.

  std::vector<std::unique_ptr<CopyOutputRequest>>& copy_requests =
      active_frame_data_->frame.render_pass_list.back()->copy_requests;

  if (copy_request->has_source()) {
    const base::UnguessableToken& source = copy_request->source();
    // Remove existing CopyOutputRequests made on the Surface by the same
    // source.
    base::EraseIf(copy_requests,
                  [&source](const std::unique_ptr<CopyOutputRequest>& x) {
                    return x->has_source() && x->source() == source;
                  });
  }
  copy_requests.push_back(std::move(copy_request));
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

void Surface::ActivatePendingFrame() {
  DCHECK(pending_frame_data_);
  FrameData frame_data = std::move(*pending_frame_data_);
  pending_frame_data_.reset();

  base::Optional<base::TimeDelta> duration = deadline_->Cancel();

  ActivateFrame(std::move(frame_data), std::move(duration));
}

void Surface::UpdateReferencedAllocationGroups(
    std::vector<SurfaceAllocationGroup*> new_referenced_allocation_groups) {
  base::flat_set<SurfaceAllocationGroup*> new_set(
      new_referenced_allocation_groups);

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
  last_surface_id_for_range_.clear();
  std::vector<SurfaceAllocationGroup*> new_referenced_allocation_groups;
  for (const SurfaceRange& surface_range :
       active_frame_data_->frame.metadata.referenced_surfaces) {
    // Figure out what surface in the |surface_range| needs to be referenced.
    Surface* surface =
        surface_manager_->GetLatestInFlightSurface(surface_range);
    if (surface) {
      active_referenced_surfaces_.insert(surface->surface_id());
      last_surface_id_for_range_.push_back(surface->surface_id());
    } else {
      last_surface_id_for_range_.push_back(SurfaceId());
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
  UpdateReferencedAllocationGroups(std::move(new_referenced_allocation_groups));
  UpdateSurfaceReferences();
}

// A frame is activated if all its Surface ID dependences are active or a
// deadline has hit and the frame was forcibly activated. |duration| is a
// measure of the time the frame has spent waiting on dependencies to arrive.
// If |duration| is base::nullopt, then that indicates that this frame was not
// blocked on dependencies.
void Surface::ActivateFrame(FrameData frame_data,
                            base::Optional<base::TimeDelta> duration) {
  TRACE_EVENT1("viz", "Surface::ActivateFrame", "FrameSinkId",
               surface_id().frame_sink_id().ToString());

  // Save root pass copy requests.
  std::vector<std::unique_ptr<CopyOutputRequest>> old_copy_requests;
  if (active_frame_data_) {
    std::swap(old_copy_requests,
              active_frame_data_->frame.render_pass_list.back()->copy_requests);
  }

  ClearCopyRequests();

  TakeActiveLatencyInfo(&frame_data.frame.metadata.latency_info);

  base::Optional<FrameData> previous_frame_data = std::move(active_frame_data_);

  active_frame_data_ = std::move(frame_data);

  // We no longer have a pending frame, so unregister self from
  // |blocking_allocation_groups_|.
  for (SurfaceAllocationGroup* group : blocking_allocation_groups_)
    group->UnregisterBlockedEmbedder(this, true /* did_activate */);
  blocking_allocation_groups_.clear();

  RecomputeActiveReferencedSurfaces();

  for (auto& copy_request : old_copy_requests)
    RequestCopyOfOutput(std::move(copy_request));

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

  surface_manager_->SurfaceActivated(this, duration);

  // Defer notifying the embedder of an updated token until the frame has been
  // completely processed.
  const auto& metadata = GetActiveFrame().metadata;
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

  const base::Optional<uint32_t>& default_deadline =
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

  base::flat_set<SurfaceAllocationGroup*> new_blocking_allocation_groups;
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

  for (const auto& render_pass : active_frame_data_->frame.render_pass_list) {
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
  for (std::unique_ptr<CopyOutputRequest>& request :
       surface_client_->TakeCopyOutputRequests(
           surface_id().local_surface_id())) {
    RequestCopyOfOutput(std::move(request));
  }
}

bool Surface::HasCopyOutputRequests() {
  if (!active_frame_data_)
    return false;
  for (const auto& render_pass : active_frame_data_->frame.render_pass_list) {
    if (!render_pass->copy_requests.empty())
      return true;
  }
  return false;
}

const CompositorFrame& Surface::GetActiveFrame() const {
  DCHECK(active_frame_data_);
  return active_frame_data_->frame;
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
  if (!active_frame_data_ || active_frame_data_->frame_acked)
    return;
  active_frame_data_->frame_acked = true;
  if (surface_client_)
    surface_client_->OnSurfaceProcessed(this);
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

void Surface::UnrefFrameResourcesAndRunCallbacks(
    base::Optional<FrameData> frame_data) {
  if (!frame_data || !surface_client_)
    return;

  std::vector<ReturnedResource> resources =
      TransferableResource::ReturnResources(frame_data->frame.resource_list);
  // No point in returning same sync token to sender.
  for (auto& resource : resources)
    resource.sync_token.Clear();
  surface_client_->UnrefResources(resources);

  if (!frame_data->frame_acked)
    surface_client_->OnSurfaceProcessed(this);

  // If we won't be getting a presented notification, we'll notify the client
  // when the frame is unref'd.
  if (!frame_data->will_be_notified_of_presentation && surface_client_)
    surface_client_->OnSurfacePresented(frame_data->frame.metadata.frame_token,
                                        base::TimeTicks(), gfx::SwapTimings(),
                                        gfx::PresentationFeedback::Failure());
}

void Surface::ClearCopyRequests() {
  if (active_frame_data_) {
    for (const auto& render_pass : active_frame_data_->frame.render_pass_list) {
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
  std::copy(frame->metadata.latency_info.begin(),
            frame->metadata.latency_info.end(),
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

    // Tests may not be sending valid time stamps.
    // Additionally since the allocation time is not a member of LocalSurfaceId
    // it has to be added to each new site that is sneding LocalSurfaceIds to
    // Viz. Due to this, new embedders may initially be sending invalid time
    // stamps. Do not calculate metrics for those.
    if (!active_frame_data_->frame.metadata.local_surface_id_allocation_time
             .is_null()) {
      // Only send UMAs if we can calculate a valid delta.
      base::TimeDelta delta =
          base::TimeTicks::Now() -
          active_frame_data_->frame.metadata.local_surface_id_allocation_time;
      base::UmaHistogramTimes("Viz.DisplayCompositor.SurfaceEmbeddingTime",
                              delta);
    }

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
  DCHECK(HasPendingFrame());
  // Deadline inheritance implies that this surface was blocking the embedder,
  // so there shouldn't be an active frame.
  DCHECK(!HasActiveFrame());
  ActivatePendingFrameForDeadline();
}

}  // namespace viz
