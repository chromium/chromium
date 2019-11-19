// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_OFFSCREEN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_OFFSCREEN_H_

#include <memory>

#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display_embedder/gl_output_surface.h"
#include "components/viz/service/display_embedder/viz_process_context_provider.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/color_space.h"

namespace viz {

// An OutputSurface implementation that draws and swaps to an offscreen GL
// framebuffer.
class VIZ_SERVICE_EXPORT GLOutputSurfaceOffscreen : public GLOutputSurface {
 public:
  explicit GLOutputSurfaceOffscreen(
      scoped_refptr<VizProcessContextProvider> context_provider);
  ~GLOutputSurfaceOffscreen() override;

  // OutputSurface implementation.
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool alpha,
               bool stencil) override;
  void SwapBuffers(OutputSurfaceFrame frame) override;

 private:
  void OnSwapBuffersComplete(std::vector<ui::LatencyInfo> latency_info);

  gpu::Mailbox mailbox_;

  uint32_t fbo_ = 0;
  uint32_t texture_id_ = 0;
  gfx::Size size_;
  gfx::ColorSpace color_space_;

  base::WeakPtrFactory<GLOutputSurfaceOffscreen> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GLOutputSurfaceOffscreen);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_OFFSCREEN_H_
