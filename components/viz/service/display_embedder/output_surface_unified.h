// Copyright 2019 The Chromium Authors
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
// OutputSurfaceUnified will end up with a corresponding NullRenderer. While
// Chrome OS uses GL rendering to draw it doesn't matter what renderer is
// created for the unified display because it's never used to draw. Using
// NullRenderer avoids the need to allocate a GL context and command buffer,
// which have significant memory overhead.
class OutputSurfaceUnified : public OutputSurface {
 public:
  // TODO(kylechar): Add test that uses OutputSurfaceUnified.
  OutputSurfaceUnified();

  OutputSurfaceUnified(const OutputSurfaceUnified&) = delete;
  OutputSurfaceUnified& operator=(const OutputSurfaceUnified&) = delete;

  ~OutputSurfaceUnified() override;

  // OutputSurface implementation.
  void BindToClient(OutputSurfaceClient* client) override {}
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void Reshape(const ReshapeParams& params) override {}
  void SwapBuffers(OutputSurfaceFrame frame) override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override {}
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_SURFACE_UNIFIED_H_
