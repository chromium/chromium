// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_ALLOCATION_GROUP_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_ALLOCATION_GROUP_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/latency/latency_info.h"

namespace viz {

class Surface;
class SurfaceManager;

// This class keeps track of the LocalSurfaceIds that were generated using the
// same ParentLocalSurfaceIdAllocator (i.e. have the same embed token).
// A SurfaceAllocationGroup is created when:
// - A surface is created with an embed token that was never seen before, OR
// - A surface embeds another surface that has an embed token that was never
//   seen before.
// Once all the surfaces in the allocation group and all of the embedders are
// unregistered, the allocation group will be garbage-collected.
class VIZ_SERVICE_EXPORT SurfaceAllocationGroup {
 public:
  SurfaceAllocationGroup(SurfaceManager* surface_manager,
                         const FrameSinkId& submitter,
                         const base::UnguessableToken& embed_token);

  SurfaceAllocationGroup(const SurfaceAllocationGroup&) = delete;
  SurfaceAllocationGroup& operator=(const SurfaceAllocationGroup&) = delete;

  ~SurfaceAllocationGroup();

  // Returns the ID of the FrameSink that is submitting to surfaces in this
  // allocation group.
  const FrameSinkId& submitter_frame_sink_id() const { return submitter_; }

  // Returns whether this SurfaceAllocationGroup can be destroyed by the garbage
  // collector; that is, there are no surfaces left in this allocation group and
  // there are no registered embedders.
  bool IsReadyToDestroy() const;

  // Called by |surface| at construction time to register itself in this
  // allocation group.
  void RegisterSurface(Surface* surface);

  // Called by |surface| at destruction time to unregister itself from this
  // allocation group.
  void UnregisterSurface(Surface* surface);

  // Called by |surface| when it has a pending frame that is blocked on
  // |activation_dependency| in this allocation group. The embedder will be
  // notified when |activation_dependency| becomes available.
  void RegisterBlockedEmbedder(Surface* surface,
                               const SurfaceId& activation_dependency);

  // Called by |surface| when its pending frame that still has an unresolved
  // activation dependency in this allocation group either activates
  // (|did_activate| == true) or gets dropped (|did_activate| == false).
  void UnregisterBlockedEmbedder(Surface* surface, bool did_activate);

  // Returns whether there is any embedder that is blocked on a surface in this
  // allocation group.
  bool HasBlockedEmbedder() const;

  // Called by |surface| when its newly activated frame references a surface in
  // this allocation group. The embedder will be notified whenever a surface in
  // this allocation group activates for the first time.
  void RegisterActiveEmbedder(Surface* surface);

  // Called by |surface| when it no longer has an active frame that references a
  // surface in this allocation group.
  void UnregisterActiveEmbedder(Surface* surface);

  // Notifies that a surface exists whose active frame references |surface_id|
  // in this allocation group. |surface_id| or the last surface prior to it may
  // be activated due to deadline inheritance.
  void UpdateLastActiveReferenceAndMaybeActivate(const SurfaceId& surface_id);

  // Notifies that a surface exists whose pending frame references |surface_id|
  // in this allocation group. |surface_id| or some surface prior to it might
  // activate if it was blocked due to child throttling.
  void UpdateLastPendingReferenceAndMaybeActivate(const SurfaceId& surface_id);

  // Returns the last SurfaceId in this allocation group that was ever
  // referenced by the active frame of a surface.
  const SurfaceId& GetLastActiveReference();

  // Returns the last SurfaceId in this allocation group that was ever
  // referenced by a pending or an active frame of a surface.
  const SurfaceId& GetLastReference();

  // Returns the latest active surface in the given range that is a part of this
  // allocation group. The embed token of at least one end of the range must
  // match the embed token of this group.
  Surface* FindLatestActiveSurfaceInRange(const SurfaceRange& range) const;

  // Takes the LatencyInfo of the active frame of |surface|, plus the
  // LatencyInfo of both pending and active frames of every surface older than
  // |surface|.
  void TakeAggregatedLatencyInfoUpTo(Surface* surface,
                                     std::vector<ui::LatencyInfo>* out);

  // Called by the surfaces in this allocation when they activate for the first
  // time.
  void OnFirstSurfaceActivation(Surface* surface);

  // Called when there will not be any calls to RegisterSurface in the future.
  // All pending embedders that were blocked on surfaces that don't exist yet
  // will have their dependency resolved.
  void WillNotRegisterNewSurfaces();

  // Called by surfaces which are blocked by this allocation group. This will
  // send an Ack to the latest active surface, if it has an un-Acked frame.
  void AckLastestActiveUnAckedFrame();

  // Returns the last surface created in this allocation group.
  Surface* last_created_surface() const {
    return surfaces_.empty() ? nullptr : surfaces_.back();
  }

  const std::vector<raw_ptr<Surface, VectorExperimental>>& surfaces() const {
    return surfaces_;
  }

 private:
  // Returns an iterator to the latest surface in |surfaces_| whose SurfaceId is
  // older than or equal to |surface_id|. The returned surface may not be active
  // yet.
  std::vector<raw_ptr<Surface, VectorExperimental>>::const_iterator
  FindLatestSurfaceUpTo(const SurfaceId& surface_id) const;

  // Returns an iterator to the latest active surface in |surfaces_| whose
  // SurfaceId is older than or equal to |surface_id|.
  std::vector<raw_ptr<Surface, VectorExperimental>>::const_iterator
  FindLatestActiveSurfaceUpTo(const SurfaceId& surface_id) const;

  // Notifies SurfaceManager if this allocation group is ready for destruction
  // (see IsReadyToDestroy() for the requirements).
  void MaybeMarkForDestruction();

  // Updates the last reference. |surface_id| or a surface prior to it might
  // activate if it was blocked due to child throttling.
  void UpdateLastReferenceAndMaybeActivate(const SurfaceId& surface_id);

  // The ID of the FrameSink that is submitting to the surfaces in this
  // allocation group.
  const FrameSinkId submitter_;

  // The embed token that the ParentLocalSurfaceIdAllocator used to allocate
  // LocalSurfaceIds for the surfaces in this allocation group. All the surfaces
  // in this allocation group use this embed token.
  const base::UnguessableToken embed_token_;

  // The list of surfaces in this allocation group in the order of creation. The
  // parent and child sequence numbers of these surfaces is monotonically
  // increasing.
  std::vector<raw_ptr<Surface, VectorExperimental>> surfaces_;

  // A map from the surfaces that have an unresolved activation dependency in
  // this allocation group, to the said activation dependency.
  base::flat_map<Surface*, SurfaceId> blocked_embedders_;

  // The set of surfaces that reference a surface in this allocation group by
  // their active frame.
  base::flat_set<raw_ptr<Surface, CtnExperimental>> active_embedders_;

  // We keep a pointer to SurfaceManager so we can signal when this object is
  // ready to be destroyed.
  const raw_ptr<SurfaceManager> surface_manager_;

  // The last SurfaceId of this allocation group that was ever referenced by the
  // active frame of a surface.
  SurfaceId last_active_reference_;

  // The last SurfaceId of this allocation group that was ever referenced by the
  // active or pending frame of a surface.
  SurfaceId last_reference_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_ALLOCATION_GROUP_H_
