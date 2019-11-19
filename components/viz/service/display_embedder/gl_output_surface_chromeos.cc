// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_chromeos.h"

namespace viz {

GLOutputSurfaceChromeOS::GLOutputSurfaceChromeOS(
    scoped_refptr<VizProcessContextProvider> context_provider,
    gpu::SurfaceHandle surface_handle)
    : GLOutputSurface(context_provider, surface_handle) {}

GLOutputSurfaceChromeOS::~GLOutputSurfaceChromeOS() = default;

void GLOutputSurfaceChromeOS::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  display_transform_ = transform;
}

gfx::OverlayTransform GLOutputSurfaceChromeOS::GetDisplayTransform() {
  return display_transform_;
}

}  // namespace viz
