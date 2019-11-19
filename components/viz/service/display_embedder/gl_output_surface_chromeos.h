// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_CHROMEOS_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_CHROMEOS_H_

#include "components/viz/service/display_embedder/gl_output_surface.h"

namespace viz {

class GLOutputSurfaceChromeOS : public GLOutputSurface {
 public:
  GLOutputSurfaceChromeOS(
      scoped_refptr<VizProcessContextProvider> context_provider,
      gpu::SurfaceHandle surface_handle);
  ~GLOutputSurfaceChromeOS() override;

  // GLOutputSurface:
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override;
  gfx::OverlayTransform GetDisplayTransform() override;

 private:
  gfx::OverlayTransform display_transform_ = gfx::OVERLAY_TRANSFORM_NONE;

  DISALLOW_COPY_AND_ASSIGN(GLOutputSurfaceChromeOS);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_CHROMEOS_H_
