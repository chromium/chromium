// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_dependency_tracker.h"

#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"

namespace viz {

SurfaceDependencyTracker::SurfaceDependencyTracker(
    SurfaceManager* surface_manager)
    : surface_manager_(surface_manager) {}

SurfaceDependencyTracker::~SurfaceDependencyTracker() = default;

void SurfaceDependencyTracker::TrackEmbedding(Surface* surface) {
  // If |surface| is blocking on the arrival of a parent and the parent frame
  // has not yet arrived then track this |surface|'s SurfaceId by FrameSinkId so
  // that if a parent refers to it or a more recent surface, then
  // SurfaceDependencyTracker reports back that a dependency has been added.
  if (surface->block_activation_on_parent() && !surface->HasDependentFrame()) {
    surfaces_blocked_on_parent_by_frame_sink_id_[surface->surface_id()
                                                     .frame_sink_id()]
        .insert(surface->surface_id());
  }
}

void SurfaceDependencyTracker::RequestSurfaceResolution(Surface* surface) {
  DCHECK(surface->HasPendingFrame());

  if (IsSurfaceLate(surface)) {
    ActivateLateSurfaceSubtree(surface);
    return;
  }

  // Activation dependencies that aren't currently known to the surface manager
  // or do not have an active CompositorFrame block this frame.
  for (const SurfaceId& surface_id : surface->activation_dependencies()) {
    Surface* dependency = surface_manager_->GetSurfaceForId(surface_id);
    if (!dependency || !dependency->HasActiveFrame()) {
      blocked_surfaces_from_dependency_[surface_id.frame_sink_id()].insert(
          surface->surface_id());
    }
  }

  UpdateSurfaceDeadline(surface);
}

bool SurfaceDependencyTracker::HasSurfaceBlockedOn(
    const SurfaceId& surface_id) const {
  auto it = blocked_surfaces_from_dependency_.find(surface_id.frame_sink_id());
  if (it == blocked_surfaces_from_dependency_.end())
    return false;

  for (const SurfaceId& blocked_surface_by_id : it->second) {
    Surface* blocked_surface =
        surface_manager_->GetSurfaceForId(blocked_surface_by_id);
    if (blocked_surface && blocked_surface->IsBlockedOn(surface_id))
      return true;
  }
  return false;
}

void SurfaceDependencyTracker::OnSurfaceActivated(Surface* surface) {
  if (!surface->late_activation_dependencies().empty())
    surfaces_with_missing_dependencies_.insert(surface->surface_id());
  else
    surfaces_with_missing_dependencies_.erase(surface->surface_id());
  NotifySurfaceIdAvailable(surface->surface_id());
  // We treat an activation (by deadline) as being the equivalent of a parent
  // embedding the surface.
  OnSurfaceDependencyAdded(surface->surface_id());
}

void SurfaceDependencyTracker::OnSurfaceDependencyAdded(
    const SurfaceId& surface_id) {
  auto it = surfaces_blocked_on_parent_by_frame_sink_id_.find(
      surface_id.frame_sink_id());
  if (it == surfaces_blocked_on_parent_by_frame_sink_id_.end())
    return;

  std::vector<SurfaceId> dependencies_to_notify;

  base::flat_set<SurfaceId>& blocked_surfaces = it->second;
  for (auto iter = blocked_surfaces.begin(); iter != blocked_surfaces.end();) {
    if (iter->local_surface_id() <= surface_id.local_surface_id()) {
      dependencies_to_notify.push_back(*iter);
      iter = blocked_surfaces.erase(iter);
    } else {
      ++iter;
    }
  }

  if (blocked_surfaces.empty())
    surfaces_blocked_on_parent_by_frame_sink_id_.erase(it);

  for (const SurfaceId& dependency : dependencies_to_notify) {
    Surface* surface = surface_manager_->GetSurfaceForId(dependency);
    if (surface)
      surface->OnSurfaceDependencyAdded();
  }
}

void SurfaceDependencyTracker::OnSurfaceDependenciesChanged(
    Surface* surface,
    const base::flat_set<FrameSinkId>& added_dependencies,
    const base::flat_set<FrameSinkId>& removed_dependencies) {
  // Update the |blocked_surfaces_from_dependency_| map with the changes in
  // dependencies.
  for (const FrameSinkId& frame_sink_id : added_dependencies) {
    blocked_surfaces_from_dependency_[frame_sink_id].insert(
        surface->surface_id());
  }

  for (const FrameSinkId& frame_sink_id : removed_dependencies) {
    auto it = blocked_surfaces_from_dependency_.find(frame_sink_id);
    if (it != blocked_surfaces_from_dependency_.end()) {
      it->second.erase(surface->surface_id());
      if (it->second.empty())
        blocked_surfaces_from_dependency_.erase(it);
    }
  }
}

void SurfaceDependencyTracker::OnSurfaceDiscarded(Surface* surface) {
  surfaces_with_missing_dependencies_.erase(surface->surface_id());

  base::flat_set<FrameSinkId> removed_dependencies;
  for (const SurfaceId& surface_id : surface->activation_dependencies())
    removed_dependencies.insert(surface_id.frame_sink_id());

  OnSurfaceDependenciesChanged(surface, {}, removed_dependencies);

  // Pretend that the discarded surface's SurfaceId is now available to
  // unblock dependencies because we now know the surface will never activate.
  NotifySurfaceIdAvailable(surface->surface_id());
  OnSurfaceDependencyAdded(surface->surface_id());
}

void SurfaceDependencyTracker::OnFrameSinkInvalidated(
    const FrameSinkId& frame_sink_id) {
  // We now know the frame sink will never generated any more frames,
  // thus unblock all dependencies to any future surfaces.
  NotifySurfaceIdAvailable(SurfaceId::MaxSequenceId(frame_sink_id));
  OnSurfaceDependencyAdded(SurfaceId::MaxSequenceId(frame_sink_id));
}

void SurfaceDependencyTracker::ActivateLateSurfaceSubtree(Surface* surface) {
  DCHECK(surface->HasPendingFrame());

  base::flat_set<SurfaceId> late_dependencies(
      surface->activation_dependencies());
  for (const SurfaceId& surface_id : late_dependencies) {
    Surface* dependency = surface_manager_->GetSurfaceForId(surface_id);
    if (dependency && dependency->HasPendingFrame())
      ActivateLateSurfaceSubtree(dependency);
  }

  surface->ActivatePendingFrameForDeadline(base::nullopt);
}

void SurfaceDependencyTracker::UpdateSurfaceDeadline(Surface* surface) {
  DCHECK(surface->HasPendingFrame());

  // Inherit the deadline from the first parent blocked on this surface.
  auto it = blocked_surfaces_from_dependency_.find(
      surface->surface_id().frame_sink_id());
  if (it != blocked_surfaces_from_dependency_.end()) {
    const base::flat_set<SurfaceId>& dependent_parent_ids = it->second;
    for (const SurfaceId& parent_id : dependent_parent_ids) {
      Surface* parent = surface_manager_->GetSurfaceForId(parent_id);
      if (parent && parent->has_deadline() &&
          parent->activation_dependencies().count(surface->surface_id())) {
        surface->InheritActivationDeadlineFrom(parent);
        break;
      }
    }
  }

  DCHECK(!surface_manager_->activation_deadline_in_frames() ||
         surface->has_deadline());

  // Recursively propagate the newly set deadline to children.
  base::flat_set<SurfaceId> activation_dependencies(
      surface->activation_dependencies());
  for (const SurfaceId& surface_id : activation_dependencies) {
    Surface* dependency = surface_manager_->GetSurfaceForId(surface_id);
    if (dependency && dependency->HasPendingFrame())
      UpdateSurfaceDeadline(dependency);
  }
}

bool SurfaceDependencyTracker::IsSurfaceLate(Surface* surface) {
  for (const SurfaceId& surface_id : surfaces_with_missing_dependencies_) {
    Surface* activated_surface = surface_manager_->GetSurfaceForId(surface_id);
    DCHECK(activated_surface->HasActiveFrame());
    if (activated_surface->late_activation_dependencies().count(
            surface->surface_id())) {
      return true;
    }
  }
  return false;
}

void SurfaceDependencyTracker::NotifySurfaceIdAvailable(
    const SurfaceId& surface_id) {
  auto it = blocked_surfaces_from_dependency_.find(surface_id.frame_sink_id());
  if (it == blocked_surfaces_from_dependency_.end())
    return;

  // Unblock surfaces that depend on this |surface_id|.
  base::flat_set<SurfaceId> blocked_surfaces_by_id(it->second);

  // Tell each surface about the availability of its blocker.
  for (const SurfaceId& blocked_surface_by_id : blocked_surfaces_by_id) {
    Surface* blocked_surface =
        surface_manager_->GetSurfaceForId(blocked_surface_by_id);
    if (!blocked_surface) {
      // A blocked surface may have been garbage collected during dependency
      // resolution.
      continue;
    }
    blocked_surface->NotifySurfaceIdAvailable(surface_id);
  }
}

}  // namespace viz
