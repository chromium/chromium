// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class Surface;

class VIZ_SERVICE_EXPORT SurfaceSavedFrame {
 public:
  explicit SurfaceSavedFrame(
      const CompositorFrameTransitionDirective& directive);
  ~SurfaceSavedFrame();

  // Returns true iff the frame is valid and complete.
  bool IsValid() const;

  // Returns the animation duration from the saved directive.
  base::TimeDelta animation_duration() const { return directive_.duration(); }

  // Appends copy output requests to the needed render passes in the active
  // frame.
  void RequestCopyOfOutput(Surface* surface);

  // For testing functionality that ensures that we have a valid frame.
  void CompleteSavedFrameForTesting();

 private:
  void NotifyCopyOfOutputComplete(std::unique_ptr<CopyOutputResult> result);

  CompositorFrameTransitionDirective directive_;

  CopyOutputResult::TextureResult texture_result_;
  std::unique_ptr<SingleReleaseCallback> texture_release_callback_;

  base::WeakPtrFactory<SurfaceSavedFrame> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_H_
