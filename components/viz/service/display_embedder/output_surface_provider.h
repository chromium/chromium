// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_SURFACE_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_SURFACE_PROVIDER_H_

#include <memory>

#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/gpu_task_scheduler_helper.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"

namespace gpu {
class SharedImageManager;
}

namespace viz {

struct DebugRendererSettings;
class RendererSettings;
class OutputSurface;

// Handles creating OutputSurface for FrameSinkManagerImpl.
class OutputSurfaceProvider {
 public:
  virtual ~OutputSurfaceProvider() {}

  // Needs to be called before calling the CreateOutputSurface function. Output
  // of this should feed into the CreateOutputSurface function.
  virtual std::unique_ptr<gpu::GpuTaskSchedulerHelper> CreateGpuTaskScheduler(
      bool gpu_compositing,
      const RendererSettings& renderer_settings) = 0;

  // Creates a new OutputSurface for |surface_handle|. If creating an
  // OutputSurface fails this function will return null.
  virtual std::unique_ptr<OutputSurface> CreateOutputSurface(
      gpu::SurfaceHandle surface_handle,
      bool gpu_compositing,
      mojom::DisplayClient* display_client,
      gpu::GpuTaskSchedulerHelper* gpu_task_scheduler,
      const RendererSettings& renderer_settings,
      const DebugRendererSettings* debug_settings) = 0;

  // TODO(weiliangc): This API is unfortunately located since this is the
  // overlapping place that both GLOutputSurface and SkiaOutputSurface code path
  // has access to SharedImageManager. Refactor so that OverlayProcessor and
  // OutputSurface could be initialized together at appropriate place.
  virtual gpu::SharedImageManager* GetSharedImageManager() = 0;
};

}  // namespace viz

#endif  //  COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_SURFACE_PROVIDER_H_
