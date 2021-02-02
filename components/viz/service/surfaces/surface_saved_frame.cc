// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_saved_frame.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"

namespace viz {

SurfaceSavedFrame::SurfaceSavedFrame(
    const CompositorFrameTransitionDirective& directive)
    : directive_(directive) {
  // We should only be constructing a saved frame from a save directive.
  DCHECK_EQ(directive.type(), CompositorFrameTransitionDirective::Type::kSave);
}

bool SurfaceSavedFrame::IsValid() const {
  // TODO(vmpstr): Verify that the surface frame is valid and has all of the
  // textures received from copy output requests.
  return true;
}

}  // namespace viz
