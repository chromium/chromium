// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_surface_unified.h"

#include <memory>

#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/software_output_device.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/overlay_transform.h"

namespace viz {

OutputSurfaceUnified::OutputSurfaceUnified()
    : OutputSurface(std::make_unique<SoftwareOutputDevice>()) {
  capabilities_.skips_draw = true;
}

OutputSurfaceUnified::~OutputSurfaceUnified() = default;

void OutputSurfaceUnified::SwapBuffers(OutputSurfaceFrame frame) {
  // This OutputSurface is not intended to be drawn into and should never swap.
  NOTREACHED_IN_MIGRATION();
}

gfx::OverlayTransform OutputSurfaceUnified::GetDisplayTransform() {
  return gfx::OVERLAY_TRANSFORM_NONE;
}
}  // namespace viz
