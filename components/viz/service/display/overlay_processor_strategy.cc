// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_strategy.h"

#include "components/viz/common/display/overlay_strategy.h"

namespace viz {

// Default implementation of whether a strategy would remove the output surface
// as overlay plane.
bool OverlayProcessorStrategy::RemoveOutputSurfaceAsOverlay() {
  return false;
}

OverlayStrategy OverlayProcessorStrategy::GetUMAEnum() const {
  return OverlayStrategy::kUnknown;
}

gfx::RectF OverlayProcessorStrategy::GetPrimaryPlaneDisplayRect(
    const PrimaryPlane* primary_plane) {
  return primary_plane ? primary_plane->display_rect : gfx::RectF();
}

}  // namespace viz
