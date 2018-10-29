// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_TRACKER_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_TRACKER_H_

#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class SurfaceManager;

// SurfaceDependencyTracker tracks unresolved dependencies blocking
// CompositorFrames from activating. This class maintains a map from
// a dependent surface ID to a set of Surfaces that have CompositorFrames
// blocked on that surface ID. SurfaceDependencyTracker observes when
// dependent frames activate, and informs blocked surfaces.
//
// When a blocking CompositorFrame is first submitted,
// SurfaceDependencyTracker will begin listening for BeginFrames, setting a
// deadline some number of BeginFrames in the future. If there are unresolved
// dependencies when the deadline hits, then SurfaceDependencyTracker will clear
// then and activate all pending CompositorFrames. Once there are no more
// remaining pending frames, then SurfaceDependencyTracker will stop observing
// BeginFrames.
class VIZ_SERVICE_EXPORT SurfaceDependencyTracker {
 public:
  explicit SurfaceDependencyTracker(SurfaceManager* surface_manager);
  ~SurfaceDependencyTracker();

  // Called when |surface| wishes to track when it is embedded.
  void TrackEmbedding(Surface* surface);

  // Called when |surface| has a pending CompositorFrame and it wishes to be
  // informed when that surface's dependencies are resolved.
  void RequestSurfaceResolution(Surface* surface);

  // Returns whether the dependency tracker has a surface blocked on the
  // provided |surface_id|.
  bool HasSurfaceBlockedOn(const SurfaceId& surface_id) const;

  void OnSurfaceActivated(Surface* surface);
  void OnSurfaceDependencyAdded(const SurfaceId& surface_id);
  void OnSurfaceDependenciesChanged(
      Surface* surface,
      const base::flat_set<FrameSinkId>& added_dependencies,
      const base::flat_set<FrameSinkId>& removed_dependencies);
  void OnSurfaceDiscarded(Surface* surface);
  void OnFrameSinkInvalidated(const FrameSinkId& frame_sink_id);

 private:
  // If |surface| has a dependent embedder frame, then it inherits the parent's
  // deadline and propagates that deadline to children.
  void UpdateSurfaceDeadline(Surface* surface);

  // Activates this |surface| and its entire dependency tree.
  void ActivateLateSurfaceSubtree(Surface* surface);

  // Indicates whether |surface| is late. A surface is late if it hasn't had its
  // first activation before a embedder is forced to activate its own
  // CompositorFrame. A surface may no longer be considered late if the set
  // of activation dependencies for dependent surfaces change.
  bool IsSurfaceLate(Surface* surface);

  // Informs all Surfaces with pending frames blocked on the provided
  // |surface_id| that there is now an active frame available in Surface
  // corresponding to |surface_id|.
  void NotifySurfaceIdAvailable(const SurfaceId& surface_id);

  SurfaceManager* const surface_manager_;

  // A map from a FrameSinkId to the set of Surfaces that are blocked on
  // surfaces associated with that FrameSinkId.
  std::unordered_map<FrameSinkId, base::flat_set<SurfaceId>, FrameSinkIdHash>
      blocked_surfaces_from_dependency_;

  // A map from a FrameSinkid to a set of surfaces with that FrameSinkId that
  // are blocked on a parent arriving to embed them.
  std::unordered_map<FrameSinkId, base::flat_set<SurfaceId>, FrameSinkIdHash>
      surfaces_blocked_on_parent_by_frame_sink_id_;

  // The set of SurfaceIds corresponding to Surfaces that have active
  // CompositorFrames with missing dependencies.
  base::flat_set<SurfaceId> surfaces_with_missing_dependencies_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceDependencyTracker);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_TRACKER_H_
