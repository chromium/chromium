// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_H_

#include "base/time/time.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class VIZ_SERVICE_EXPORT SurfaceSavedFrame {
 public:
  explicit SurfaceSavedFrame(
      const CompositorFrameTransitionDirective& directive);

  // Returns true iff the frame is valid and complete.
  bool IsValid() const;

  // Returns the animation duration from the saved directive.
  base::TimeDelta animation_duration() const { return directive_.duration(); }

 private:
  CompositorFrameTransitionDirective directive_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_H_
