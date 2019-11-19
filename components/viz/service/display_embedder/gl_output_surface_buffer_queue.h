// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_BUFFER_QUEUE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_BUFFER_QUEUE_H_

#include <stdint.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display_embedder/gl_output_surface.h"
#include "components/viz/service/display_embedder/viz_process_context_provider.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace viz {

class BufferQueue;

// An OutputSurface implementation that directly draws and swap to a GL
// "buffer_queue" surface (aka one backed by a buffer managed explicitly in
// mus/ozone. This class is adapted from
// GpuBufferQueueBrowserCompositorOutputSurface.
class GLOutputSurfaceBufferQueue : public GLOutputSurface {
 public:
  GLOutputSurfaceBufferQueue(
      scoped_refptr<VizProcessContextProvider> context_provider,
      gpu::SurfaceHandle surface_handle,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gfx::BufferFormat buffer_format);

  ~GLOutputSurfaceBufferQueue() override;

  // TODO(rjkroege): Implement the equivalent of Reflector.

 protected:
  // OutputSurface implementation.
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override;
  gfx::OverlayTransform GetDisplayTransform() override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;

 private:
  // OutputSurface implementation.
  void BindFramebuffer() override;
  void SwapBuffers(OutputSurfaceFrame frame) override;
  gfx::Rect GetCurrentFramebufferDamage() const override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;

  // GLOutputSurface:
  void DidReceiveSwapBuffersAck(const gfx::SwapResponse& response) override;

  std::unique_ptr<BufferQueue> buffer_queue_;
  unsigned current_texture_;
  uint32_t fbo_;

  gfx::OverlayTransform display_transform_ = gfx::OVERLAY_TRANSFORM_NONE;
  gfx::Size reshape_size_;
  gfx::Size swap_size_;

  DISALLOW_COPY_AND_ASSIGN(GLOutputSurfaceBufferQueue);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_BUFFER_QUEUE_H_
