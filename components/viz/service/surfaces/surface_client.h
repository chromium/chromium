// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_CLIENT_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_CLIENT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/viz/service/viz_service_export.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace gfx {
struct PresentationFeedback;
class Rect;
struct SwapTimings;
}  // namespace gfx

namespace viz {
struct ReturnedResource;
class CompositorFrame;
class CopyOutputRequest;
class LocalSurfaceId;
class Surface;
struct TransferableResource;

class VIZ_SERVICE_EXPORT SurfaceClient {
 public:
  SurfaceClient() = default;

  virtual ~SurfaceClient() = default;

  // Called when |surface| has a new CompositorFrame available for display.
  virtual void OnSurfaceActivated(Surface* surface) = 0;

  // Called when |surface| is about to be destroyed.
  virtual void OnSurfaceDestroyed(Surface* surface) = 0;

  // Called when a |surface| is about to be drawn.
  virtual void OnSurfaceWillDraw(Surface* surface) = 0;

  // Increments the reference count on resources specified by |resources|.
  virtual void RefResources(
      const std::vector<TransferableResource>& resources) = 0;

  // Decrements the reference count on resources specified by |resources|.
  virtual void UnrefResources(
      const std::vector<ReturnedResource>& resources) = 0;

  // ReturnResources gets called when the display compositor is done using the
  // resources so that the client can use them.
  virtual void ReturnResources(
      const std::vector<ReturnedResource>& resources) = 0;

  // Increments the reference count of resources received from a child
  // compositor.
  virtual void ReceiveFromChild(
      const std::vector<TransferableResource>& resources) = 0;

  // Takes all the CopyOutputRequests made at the client level that happened for
  // a LocalSurfaceId preceeding the given one.
  virtual std::vector<std::unique_ptr<CopyOutputRequest>>
  TakeCopyOutputRequests(const LocalSurfaceId& latest_surface_id) = 0;

  // Notifies the client that a frame with |token| has been activated.
  virtual void OnFrameTokenChanged(uint32_t frame_token) = 0;

  // Notifies the client that the submitted CompositorFrame has been processed
  // (where processed may mean the frame has been displayed, or discarded).
  virtual void OnSurfaceProcessed(Surface* surface) = 0;

  // Notifies the client that a frame with |token| has been presented.
  virtual void OnSurfacePresented(
      uint32_t frame_token,
      base::TimeTicks draw_start_timestamp,
      const gfx::SwapTimings& swap_timings,
      const gfx::PresentationFeedback& feedback) = 0;

  // This is called when |surface| or one of its descendents is determined to be
  // damaged at aggregation time.
  virtual void OnSurfaceAggregatedDamage(
      Surface* surface,
      const LocalSurfaceId& local_surface_id,
      const CompositorFrame& frame,
      const gfx::Rect& damage_rect,
      base::TimeTicks expected_display_time) = 0;

  // Returns whether a sync token should be generated before returning the
  // resources to the client.
  virtual bool NeedsSyncTokens() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceClient);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_CLIENT_H_
