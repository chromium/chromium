// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_SURFACE_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_SURFACE_PROVIDER_H_

#include <memory>

#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"
#include "gpu/command_buffer/service/gpu_task_scheduler_helper.h"
#include "gpu/ipc/common/surface_handle.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"

namespace gpu {
class Scheduler;
}  // namespace gpu

namespace viz {

struct DebugRendererSettings;
class DisplayCompositorMemoryAndTaskController;
class RendererSettings;
class OutputSurface;

// Handles creating OutputSurface for FrameSinkManagerImpl.
class OutputSurfaceProvider {
 public:
  virtual ~OutputSurfaceProvider() {}

  // Needs to be called before calling the CreateOutputSurface function. Output
  // of this should feed into the CreateOutputSurface function.
  virtual std::unique_ptr<DisplayCompositorMemoryAndTaskController>
  CreateGpuDependency(bool gpu_compositing,
                      gpu::SurfaceHandle surface_handle) = 0;

  // Creates a new OutputSurface for |surface_handle|. If creating an
  // OutputSurface fails this function will return null.
  virtual std::unique_ptr<OutputSurface> CreateOutputSurface(
      gpu::SurfaceHandle surface_handle,
      bool gpu_compositing,
      mojom::DisplayClient* display_client,
      DisplayCompositorMemoryAndTaskController* gpu_dependency,
      const RendererSettings& renderer_settings,
      const DebugRendererSettings* debug_settings) = 0;

  virtual gpu::SharedImageManager* GetSharedImageManager() = 0;
  virtual gpu::SyncPointManager* GetSyncPointManager() = 0;
  virtual gpu::Scheduler* GetGpuScheduler() = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_SURFACE_PROVIDER_H_
