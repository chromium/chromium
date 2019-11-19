// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_SURFACE_UNIFIED_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_SURFACE_UNIFIED_H_

#include "components/viz/service/display/output_surface.h"

namespace viz {

// An OutputSurface implementation for the Chrome OS unified desktop display.
// The unified display is a fake display that spans across multiple physical
// displays. The Display/OutputSurface for the unified display exist only to
// issue begin frames and doesn't need to do any drawing work. This class is
// essentially a stub implementation.
//
// OutputSurfaceUnified will end up with a corresponding SoftwareRenderer. While
// Chrome OS uses GL rendering to draw it doesn't matter what renderer is
// created for the unified display because it's never used to draw. Using
// SoftwareRenderer avoids the need to allocate a GL context and command buffer,
// which have significant memory overhead.
class OutputSurfaceUnified : public OutputSurface {
 public:
  // TODO(kylechar): Add test that uses OutputSurfaceUnified.
  OutputSurfaceUnified();
  ~OutputSurfaceUnified() override;

  // OutputSurface implementation.
  void BindToClient(OutputSurfaceClient* client) override {}
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override {}
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override {}
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool alpha,
               bool stencil) override {}
  void SwapBuffers(OutputSurfaceFrame frame) override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override {}
  uint32_t GetFramebufferCopyTextureFormat() override;
  unsigned UpdateGpuFence() override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override {}
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(OutputSurfaceUnified);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_SURFACE_UNIFIED_H_
