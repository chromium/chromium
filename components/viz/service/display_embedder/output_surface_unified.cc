// Copyright 2019 The Chromium Authors. All rights reserved.
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
  NOTREACHED();
}

bool OutputSurfaceUnified::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned OutputSurfaceUnified::GetOverlayTextureId() const {
  return 0;
}

gfx::BufferFormat OutputSurfaceUnified::GetOverlayBufferFormat() const {
  return gfx::BufferFormat::RGBX_8888;
}

bool OutputSurfaceUnified::HasExternalStencilTest() const {
  return false;
}

uint32_t OutputSurfaceUnified::GetFramebufferCopyTextureFormat() {
  return 0;
}

unsigned OutputSurfaceUnified::UpdateGpuFence() {
  return 0;
}

gfx::OverlayTransform OutputSurfaceUnified::GetDisplayTransform() {
  return gfx::OVERLAY_TRANSFORM_NONE;
}

}  // namespace viz
