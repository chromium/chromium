// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_OBSERVER_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_OBSERVER_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class Surface;
class SurfaceId;
class SurfaceInfo;
struct BeginFrameAck;
struct BeginFrameArgs;

class VIZ_SERVICE_EXPORT SurfaceObserver {
 public:
  virtual ~SurfaceObserver() = default;

  // Called when a CompositorFrame with a new SurfaceId activates for the first
  // time.
  virtual void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) {}

  // Called when a CompositorFrame within a surface corresponding to
  // |surface_id| activates. If the CompositorFrame was blocked on activation
  // dependencies then |duration| specifies the amount of time that frame was
  // blocked.
  virtual void OnSurfaceActivated(const SurfaceId& surface_id,
                                  base::Optional<base::TimeDelta> duration) {}

  // Called when a surface is marked for destruction (i.e. becomes a candidate
  // for garbage collection).
  virtual void OnSurfaceMarkedForDestruction(const SurfaceId& surface_id) {}

  // Called when a surface is destroyed.
  virtual void OnSurfaceDestroyed(const SurfaceId& surface_id) {}

  // Called when a Surface is modified, e.g. when a CompositorFrame is
  // activated, its producer confirms that no CompositorFrame will be submitted
  // in response to a BeginFrame, or a CopyOutputRequest is issued.
  //
  // |ack.sequence_number| is only valid if called in response to a BeginFrame.
  // Should return true if this causes a Display to be damaged.
  virtual bool OnSurfaceDamaged(const SurfaceId& surface_id,
                                const BeginFrameAck& ack);

  // Called when a Surface's CompositorFrame producer has received a BeginFrame
  // and, thus, is expected to produce damage soon.
  virtual void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                                       const BeginFrameArgs& args) {}

  // Called whenever |surface| will be drawn in the next display frame.
  virtual void OnSurfaceWillBeDrawn(Surface* surface) {}

  // Called whenever the surface reference from the surface that has |parent_id|
  // to the surface that has |child_id| is added.
  virtual void OnAddedSurfaceReference(const SurfaceId& parent_id,
                                       const SurfaceId& child_id) {}

  // Called whenever the surface reference from the surface that has |parent_id|
  // to the surface that has |child_id| is removed.
  virtual void OnRemovedSurfaceReference(const SurfaceId& parent_id,
                                         const SurfaceId& child_id) {}
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_OBSERVER_H_
