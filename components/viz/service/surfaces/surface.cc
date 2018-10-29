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
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/surfaces/referenced_surface_tracker.h"
#include "components/viz/service/surfaces/surface_client.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/presentation_feedback.h"

namespace viz {

Surface::Surface(const SurfaceInfo& surface_info,
                 SurfaceManager* surface_manager,
                 base::WeakPtr<SurfaceClient> surface_client,
                 bool needs_sync_tokens,
                 bool block_activation_on_parent)
    : surface_info_(surface_info),
      surface_manager_(surface_manager),
      surface_client_(std::move(surface_client)),
      needs_sync_tokens_(needs_sync_tokens),
      block_activation_on_parent_(block_activation_on_parent) {
  TRACE_EVENT_ASYNC_BEGIN1(TRACE_DISABLED_BY_DEFAULT("viz.surface_lifetime"),
                           "Surface", this, "surface_info",
                           surface_info.ToString());
}

Surface::~Surface() {
  ClearCopyRequests();

  if (surface_client_)
    surface_client_->OnSurfaceDiscarded(this);
  surface_manager_->SurfaceDiscarded(this);

  UnrefFrameResourcesAndRunCallbacks(std::move(pending_frame_data_));
  UnrefFrameResourcesAndRunCallbacks(std::move(active_frame_data_));

  // Remove this surface as an observer.
  for (const FrameSinkId& sink_id : observed_sinks_)
    surface_manager_->RemoveActivationObserver(sink_id, surface_info_.id());

  DCHECK(deadline_);
  deadline_->Cancel();

  TRACE_EVENT_ASYNC_END1(TRACE_DISABLED_BY_DEFAULT("viz.surface_lifetime"),
                         "Surface", this, "surface_info",
                         surface_info_.ToString());
}

void Surface::SetDependencyDeadline(
    std::unique_ptr<SurfaceDependencyDeadline> deadline) {
  deadline_ = std::move(deadline);
}

void Surface::InheritActivationDeadlineFrom(Surface* surface) {
  TRACE_EVENT1("viz", "Surface::InheritActivationDeadlineFrom", "FrameSinkId",
               surface_id().frame_sink_id().ToString());
  DCHECK(deadline_);
  DCHECK(surface->deadline_);

  deadline_->InheritFrom(*surface->deadline_);
}

void Surface::SetPreviousFrameSurface(Surface* surface) {
  DCHECK(surface && (HasActiveFrame() || HasPendingFrame()));
  previous_frame_surface_id_ = surface->surface_id();
  CompositorFrame& frame = active_frame_data_ ? active_frame_data_->frame
                                              : pending_frame_data_->frame;
  surface->TakeLatencyInfo(&frame.metadata.latency_info);
  surface->TakeLatencyInfoFromPendingFrame(&frame.metadata.latency_info);
}

void Surface::RefResources(const std::vector<TransferableResource>& resources) {
  if (surface_client_)
    surface_client_->RefResources(resources);
}

void Surface::UnrefResources(const std::vector<ReturnedResource>& resources) {
  if (surface_client_)
    surface_client_->UnrefResources(resources);
}

void Surface::RejectCompositorFramesToFallbackSurfaces() {
  for (const SurfaceRange& surface_range :
       GetPendingFrame().metadata.referenced_surfaces) {
    // Only close the fallback surface if it exists, has a different
    // LocalSurfaceId than the primary surface but has the same FrameSinkId
    // as the primary surface.
    if (!surface_range.start() ||
        surface_range.start() == surface_range.end() ||
        surface_range.start()->frame_sink_id() !=
            surface_range.end().frame_sink_id()) {
      continue;
    }
    Surface* fallback_surface =
        surface_manager_->GetLatestInFlightSurface(surface_range);

    // A misbehaving client may report a non-existent surface ID as a
    // |referenced_surface|. In that case, |surface| would be nullptr, and
    // there is nothing to do here.
    if (fallback_surface &&
        fallback_surface->surface_id() != surface_range.end()) {
      fallback_surface->Close();
    }
  }
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

void Surface::OnChildActivated(const SurfaceId& activated_id) {
  DCHECK(HasActiveFrame());

  for (size_t i = 0;
       i < active_frame_data_->frame.metadata.referenced_surfaces.size(); i++) {
    const SurfaceRange& surface_range =
        active_frame_data_->frame.metadata.referenced_surfaces[i];
    const SurfaceId& last_id = last_surface_id_for_range_[i];

    // If |activated_id| is in fallback's FrameSinkId but we already reference a
    // SurfaceId in the primary's FrameSinkId, we do nothing.
    if (surface_range.HasDifferentFrameSinkIds() && last_id.is_valid() &&
        last_id.frame_sink_id() == surface_range.end().frame_sink_id() &&
        activated_id.frame_sink_id() ==
            surface_range.start()->frame_sink_id()) {
      continue;
    }

    if (surface_range.IsInRangeInclusive(activated_id)) {
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

      // Update the referenced surface for this range.
      last_surface_id_for_range_[i] = activated_id;
    }
  }
}

void Surface::OnSurfaceDependencyAdded() {
  if (seen_first_surface_dependency_)
    return;

  seen_first_surface_dependency_ = true;
  if (!activation_dependencies_.empty() || !pending_frame_data_)
    return;

  DCHECK(frame_sink_id_dependencies_.empty());

  // All blockers have been cleared. The surface can be activated now.
  ActivatePendingFrame(base::nullopt);
}

void Surface::Close() {
  closed_ = true;
}

bool Surface::QueueFrame(
    CompositorFrame frame,
    uint64_t frame_index,
    base::ScopedClosureRunner frame_rejected_callback,
    PresentedCallback presented_callback) {
  late_activation_dependencies_.clear();

  if (frame.size_in_pixels() != surface_info_.size_in_pixels() ||
      frame.device_scale_factor() != surface_info_.device_scale_factor()) {
    TRACE_EVENT_INSTANT0("viz", "Surface invariants violation",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (closed_)
    return true;

  if (active_frame_data_ || pending_frame_data_)
    previous_frame_surface_id_ = surface_id();

  TakeLatencyInfoFromPendingFrame(&frame.metadata.latency_info);

  base::Optional<FrameData> previous_pending_frame_data =
      std::move(pending_frame_data_);
  pending_frame_data_.reset();

  UpdateActivationDependencies(frame);

  // Receive and track the resources referenced from the CompositorFrame
  // regardless of whether it's pending or active.
  surface_client_->ReceiveFromChild(frame.resource_list);

  if (!seen_first_surface_dependency_) {
    seen_first_surface_dependency_ =
        surface_manager_->dependency_tracker()->HasSurfaceBlockedOn(
            surface_id());
  }

  bool block_activation =
      block_activation_on_parent_ && !seen_first_surface_dependency_;

  if (!block_activation && activation_dependencies_.empty()) {
    // If there are no blockers, then immediately activate the frame.
    ActivateFrame(
        FrameData(std::move(frame), frame_index, std::move(presented_callback)),
        base::nullopt);
  } else {
    pending_frame_data_ =
        FrameData(std::move(frame), frame_index, std::move(presented_callback));
    RejectCompositorFramesToFallbackSurfaces();

    // Ask SurfaceDependencyTracker to inform |this| when it is embedded.
    if (block_activation)
      surface_manager_->dependency_tracker()->TrackEmbedding(this);

    // If the deadline is in the past, then the CompositorFrame will activate
    // immediately.
    if (deadline_->Set(ResolveFrameDeadline(frame))) {
      // Ask the SurfaceDependencyTracker to inform |this| when its dependencies
      // are resolved.
      surface_manager_->dependency_tracker()->RequestSurfaceResolution(this);
    }
  }

  // Returns resources for the previous pending frame.
  UnrefFrameResourcesAndRunCallbacks(std::move(previous_pending_frame_data));

  // The frame should not fail to display beyond this point. Release the
  // callback so it is not called.
  (void)frame_rejected_callback.Release();

  return true;
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

void Surface::NotifySurfaceIdAvailable(const SurfaceId& surface_id) {
  auto it = frame_sink_id_dependencies_.find(surface_id.frame_sink_id());
  if (it == frame_sink_id_dependencies_.end())
    return;

  if (surface_id.local_surface_id().parent_sequence_number() >=
          it->second.parent_sequence_number &&
      surface_id.local_surface_id().child_sequence_number() >=
          it->second.child_sequence_number) {
    frame_sink_id_dependencies_.erase(it);
    surface_manager_->SurfaceDependenciesChanged(this, {},
                                                 {surface_id.frame_sink_id()});
  }

  // LocalSurfaceIds of a given FrameSinkId are monotonically increasing in time
  // so if LocalSurfaceId j arrives then all LocalSurfaceIds i<j will never
  // arrive and so we just drop these invalid activation dependencies here.
  // TODO(fsamuel): This is a linear scan which is probably fine today because
  // a given surface has a small number of dependencies. We might need to
  // revisit this in the future if the number of dependencies grows
  // significantly.
  base::EraseIf(
      activation_dependencies_, [surface_id](const SurfaceId& dependency) {
        return dependency.frame_sink_id() == surface_id.frame_sink_id() &&
               dependency.local_surface_id() <= surface_id.local_surface_id();
      });

  // We cannot activate this CompositorFrame if there are still missing
  // activation dependencies or this surface is blocked on its parent arriving
  // and the parent has not arrived yet.
  bool block_activation =
      block_activation_on_parent_ && !seen_first_surface_dependency_;
  if (block_activation || !activation_dependencies_.empty())
    return;

  DCHECK(frame_sink_id_dependencies_.empty());

  // All blockers have been cleared. The surface can be activated now.
  ActivatePendingFrame(base::nullopt);
}

bool Surface::IsBlockedOn(const SurfaceId& surface_id) const {
  for (const SurfaceId& dependency : activation_dependencies_) {
    if (dependency.frame_sink_id() != surface_id.frame_sink_id())
      continue;

    if (dependency.local_surface_id() <= surface_id.local_surface_id())
      return true;
  }
  return false;
}

void Surface::ActivatePendingFrameForDeadline(
    base::Optional<base::TimeDelta> duration) {
  if (!pending_frame_data_)
    return;

  // If a frame is being activated because of a deadline, then clear its set
  // of blockers.
  late_activation_dependencies_ = std::move(activation_dependencies_);
  activation_dependencies_.clear();
  frame_sink_id_dependencies_.clear();
  ActivatePendingFrame(duration);
}

Surface::FrameData::FrameData(CompositorFrame&& frame,
                              uint64_t frame_index,
                              PresentedCallback presented_callback)
    : frame(std::move(frame)),
      frame_index(frame_index),
      frame_processed(false),
      presented_callback(std::move(presented_callback)) {}

Surface::FrameData::FrameData(FrameData&& other) = default;

Surface::FrameData& Surface::FrameData::operator=(FrameData&& other) = default;

Surface::FrameData::~FrameData() = default;

void Surface::ActivatePendingFrame(base::Optional<base::TimeDelta> duration) {
  DCHECK(pending_frame_data_);
  FrameData frame_data = std::move(*pending_frame_data_);
  pending_frame_data_.reset();

  DCHECK(!duration || !deadline_->has_deadline());
  if (!duration)
    duration = deadline_->Cancel();

  ActivateFrame(std::move(frame_data), duration);
}

void Surface::UpdateObservedSinks(
    const base::flat_set<FrameSinkId>& new_observed_sinks) {
  std::vector<FrameSinkId> sinks_to_remove;
  std::vector<FrameSinkId> sinks_to_add;

  for (const FrameSinkId& sink_id : new_observed_sinks)
    if (observed_sinks_.count(sink_id) == 0)
      sinks_to_add.push_back(sink_id);

  for (const FrameSinkId& sink_id : observed_sinks_)
    if (new_observed_sinks.count(sink_id) == 0)
      sinks_to_remove.push_back(sink_id);

  for (const FrameSinkId& sink_id : sinks_to_remove) {
    observed_sinks_.erase(sink_id);
    surface_manager_->RemoveActivationObserver(sink_id, surface_info_.id());
  }

  for (const FrameSinkId& sink_id : sinks_to_add) {
    observed_sinks_.insert(sink_id);
    surface_manager_->AddActivationObserver(sink_id, surface_info_.id());
  }
}

void Surface::RecomputeActiveReferencedSurfaces() {
  // Extract the latest in flight surface from the ranges in the frame then
  // notify SurfaceManager of the new references.
  active_referenced_surfaces_.clear();
  last_surface_id_for_range_.clear();
  base::flat_set<FrameSinkId> new_observed_sinks;
  for (const SurfaceRange& surface_range :
       active_frame_data_->frame.metadata.referenced_surfaces) {
    // Observe frame sinks of both endpoints of the range.
    new_observed_sinks.insert(surface_range.end().frame_sink_id());
    if (surface_range.HasDifferentFrameSinkIds())
      new_observed_sinks.insert(surface_range.start()->frame_sink_id());

    Surface* surface =
        surface_manager_->GetLatestInFlightSurface(surface_range);
    if (surface) {
      active_referenced_surfaces_.insert(surface->surface_id());
      last_surface_id_for_range_.push_back(surface->surface_id());
    } else {
      last_surface_id_for_range_.push_back(SurfaceId());
    }
  }
  UpdateObservedSinks(new_observed_sinks);
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

  TakeLatencyInfo(&frame_data.frame.metadata.latency_info);

  base::Optional<FrameData> previous_frame_data = std::move(active_frame_data_);

  active_frame_data_ = std::move(frame_data);

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
  const base::Optional<uint32_t>& default_deadline =
      surface_manager_->activation_deadline_in_frames();
  const FrameDeadline& deadline = current_frame.metadata.deadline;
  uint32_t deadline_in_frames = deadline.deadline_in_frames();

  bool block_activation =
      block_activation_on_parent_ && !seen_first_surface_dependency_;

  // If no default deadline is available then all deadlines are treated as
  // effectively infinite deadlines.
  if (!default_deadline || deadline.use_default_lower_bound_deadline() ||
      block_activation) {
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
  base::flat_map<FrameSinkId, SequenceNumbers> new_frame_sink_id_dependencies;
  base::flat_set<SurfaceId> new_activation_dependencies;

  for (const SurfaceId& surface_id :
       current_frame.metadata.activation_dependencies) {
    // Inform the Surface |dependency| that it's been added as a dependency in
    // another Surface's CompositorFrame.
    surface_manager_->SurfaceDependencyAdded(surface_id);

    Surface* dependency = surface_manager_->GetSurfaceForId(surface_id);

    // If a activation dependency does not have a corresponding active frame in
    // the display compositor, then it blocks this frame.
    if (!dependency || !dependency->HasActiveFrame()) {
      new_activation_dependencies.insert(surface_id);
      TRACE_EVENT_WITH_FLOW2(
          TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
          "LocalSurfaceId.Embed.Flow",
          TRACE_ID_GLOBAL(surface_id.local_surface_id().embed_trace_id()),
          TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
          "AddedActivationDependency", "child_surface_id",
          surface_id.ToString());

      // Record the latest |parent_sequence_number| this surface is interested
      // in observing for the provided FrameSinkId.
      uint32_t& parent_sequence_number =
          new_frame_sink_id_dependencies[surface_id.frame_sink_id()]
              .parent_sequence_number;
      parent_sequence_number =
          std::max(parent_sequence_number,
                   surface_id.local_surface_id().parent_sequence_number());

      uint32_t& child_sequence_number =
          new_frame_sink_id_dependencies[surface_id.frame_sink_id()]
              .child_sequence_number;
      child_sequence_number =
          std::max(child_sequence_number,
                   surface_id.local_surface_id().child_sequence_number());
    }
  }

  // If this Surface has a previous pending frame, then we must determine the
  // changes in dependencies so that we can update the SurfaceDependencyTracker
  // map.
  ComputeChangeInDependencies(new_frame_sink_id_dependencies);

  activation_dependencies_ = std::move(new_activation_dependencies);
  frame_sink_id_dependencies_ = std::move(new_frame_sink_id_dependencies);
}

void Surface::ComputeChangeInDependencies(
    const base::flat_map<FrameSinkId, SequenceNumbers>& new_dependencies) {
  base::flat_set<FrameSinkId> added_dependencies;
  base::flat_set<FrameSinkId> removed_dependencies;

  for (const auto& kv : frame_sink_id_dependencies_) {
    if (!new_dependencies.count(kv.first))
      removed_dependencies.insert(kv.first);
  }

  for (const auto& kv : new_dependencies) {
    if (!frame_sink_id_dependencies_.count(kv.first))
      added_dependencies.insert(kv.first);
  }

  // If there is a change in the dependency set, then inform SurfaceManager.
  if (!added_dependencies.empty() || !removed_dependencies.empty()) {
    surface_manager_->SurfaceDependenciesChanged(this, added_dependencies,
                                                 removed_dependencies);
  }
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

void Surface::TakeLatencyInfo(std::vector<ui::LatencyInfo>* latency_info) {
  if (!active_frame_data_)
    return;
  TakeLatencyInfoFromFrame(&active_frame_data_->frame, latency_info);
}

bool Surface::TakePresentedCallback(PresentedCallback* callback) {
  if (active_frame_data_ && active_frame_data_->presented_callback) {
    *callback = std::move(active_frame_data_->presented_callback);
    return true;
  }
  return false;
}

void Surface::RunDrawCallback() {
  if (!active_frame_data_ || active_frame_data_->frame_processed)
    return;
  active_frame_data_->frame_processed = true;
  if (surface_client_)
    surface_client_->OnSurfaceProcessed(this);
}

void Surface::NotifyAggregatedDamage(const gfx::Rect& damage_rect,
                                     base::TimeTicks expected_display_time) {
  if (!active_frame_data_ || !surface_client_)
    return;
  surface_client_->OnSurfaceAggregatedDamage(
      this, surface_id().local_surface_id(), active_frame_data_->frame,
      damage_rect, expected_display_time);
}

void Surface::OnDeadline(base::TimeDelta duration) {
  TRACE_EVENT1("viz", "Surface::OnDeadline", "FrameSinkId",
               surface_id().frame_sink_id().ToString());
  ActivatePendingFrameForDeadline(duration);
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

  if (!frame_data->frame_processed)
    surface_client_->OnSurfaceProcessed(this);

  if (frame_data->presented_callback) {
    std::move(frame_data->presented_callback)
        .Run(gfx::PresentationFeedback::Failure());
  }
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

void Surface::TakeLatencyInfoFromPendingFrame(
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
}

}  // namespace viz
