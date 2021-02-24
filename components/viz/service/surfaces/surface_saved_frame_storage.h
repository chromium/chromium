// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_STORAGE_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_STORAGE_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "components/viz/service/surfaces/surface_saved_frame.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class CompositorFrameTransitionDirective;
class Surface;

// This class is responsible for keeping the saved frame information for
// SurfaceAnimationManager. It is responsible for appending needed copy output
// requests into the active compositor frame so that it can save frames. It is
// also responsible for expiring saved frames after a set timeout.
class VIZ_SERVICE_EXPORT SurfaceSavedFrameStorage {
 public:
  // Each Surface has its own storage, and the storage has a backpointer to the
  // surface in order to append copy output requests to the active frame.
  explicit SurfaceSavedFrameStorage(Surface* surface);

  ~SurfaceSavedFrameStorage();

  // Processes the save directive from a compositor frame. This interfaces with
  // the Surface to append copy output requests, so it should only be called
  // after the surface with the save directive has been activated.
  void ProcessSaveDirective(
      const CompositorFrameTransitionDirective& directive);

  // This takes the saved frame stored on this storage. Returns nullptr if there
  // is no saved frame, or the frame has expired.
  std::unique_ptr<SurfaceSavedFrame> TakeSavedFrame();

  // For testing functionality.
  void ExpireForTesting();
  void CompleteForTesting();

 private:
  // This expires the saved frame, if any.
  void ExpireSavedFrame();

  Surface* const surface_;

  base::CancelableOnceClosure expiry_closure_;

  std::unique_ptr<SurfaceSavedFrame> saved_frame_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_STORAGE_H_
