// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OUTPUT_SURFACE_CLIENT_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OUTPUT_SURFACE_CLIENT_H_

#include <vector>

#include "base/functional/callback.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/latency/latency_info.h"

namespace gfx {
struct CALayerParams;
struct PresentationFeedback;
}  // namespace gfx

namespace gpu {
struct SwapBuffersCompleteParams;
}

namespace viz {

class VIZ_SERVICE_EXPORT OutputSurfaceClient {
 public:
  // A notification that the swap of the backbuffer to the hardware is complete
  // and is now visible to the user, along with information about the swap
  // including: the result, timings, what was swapped, what can be released, and
  // damage compared to the last swapped buffer.
  virtual void DidReceiveSwapBuffersAck(
      const gpu::SwapBuffersCompleteParams& params,
      gfx::GpuFenceHandle release_fence) = 0;

  // For displaying a swapped frame's contents on macOS.
  virtual void DidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) = 0;

  // For sending swap sizes back to the browser process. Currently only used on
  // Android and Linux.
  virtual void DidSwapWithSize(const gfx::Size& pixel_size) = 0;

  // See |gfx::PresentationFeedback| for detail.
  virtual void DidReceivePresentationFeedback(
      const gfx::PresentationFeedback& feedback) = 0;

  // For synchronizing IOSurface use with the macOS WindowServer with
  // SkiaRenderer.
  virtual void DidReceiveReleasedOverlays(
      const std::vector<gpu::Mailbox>& released_overlays) = 0;

  // Sends the created child window to the browser process so that it can be
  // parented to the browser process window.
  virtual void AddChildWindowToBrowser(gpu::SurfaceHandle child_window) = 0;

 protected:
  virtual ~OutputSurfaceClient() {}
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OUTPUT_SURFACE_CLIENT_H_
