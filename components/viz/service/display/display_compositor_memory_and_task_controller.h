// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_COMPOSITOR_MEMORY_AND_TASK_CONTROLLER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_COMPOSITOR_MEMORY_AND_TASK_CONTROLLER_H_

#include <memory>

#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/service/display_compositor_memory_and_task_controller_on_gpu.h"
#include "gpu/command_buffer/service/gpu_task_scheduler_helper.h"

namespace base {
class WaitableEvent;
}

namespace gpu {
class SharedImageInterface;
class SharedImageInterfaceInProcess;
}  // namespace gpu

namespace viz {
class SkiaOutputSurfaceDependency;

// This class holds ownership of task posting sequence to the gpu thread and
// memory tracking for the display compositor. This class has a 1:1 relationship
// to the display compositor class. This class is only used for gpu compositing.
// TODO(weiliangc): This should merge with SkiaOutputSurfaceDependency.
class VIZ_SERVICE_EXPORT DisplayCompositorMemoryAndTaskController {
 public:
  explicit DisplayCompositorMemoryAndTaskController(
      std::unique_ptr<SkiaOutputSurfaceDependency> skia_dependency);
  DisplayCompositorMemoryAndTaskController(
      const DisplayCompositorMemoryAndTaskController&) = delete;
  DisplayCompositorMemoryAndTaskController& operator=(
      const DisplayCompositorMemoryAndTaskController&) = delete;
  ~DisplayCompositorMemoryAndTaskController();

  SkiaOutputSurfaceDependency* skia_dependency() {
    return skia_dependency_.get();
  }
  gpu::GpuTaskSchedulerHelper* gpu_task_scheduler() {
    return gpu_task_scheduler_.get();
  }

  gpu::DisplayCompositorMemoryAndTaskControllerOnGpu* controller_on_gpu() {
    return controller_on_gpu_.get();
  }

  gpu::SharedImageInterface* shared_image_interface();

 private:
  void InitializeOnGpu(SkiaOutputSurfaceDependency* skia_dependency,
                       base::WaitableEvent* event);
  void DestroyOnGpu(base::WaitableEvent* event);

  // Accessed on viz compositor thread.
  std::unique_ptr<SkiaOutputSurfaceDependency> skia_dependency_;

  std::unique_ptr<gpu::GpuTaskSchedulerHelper> gpu_task_scheduler_;

  // Accessed on the gpu thread.
  std::unique_ptr<gpu::DisplayCompositorMemoryAndTaskControllerOnGpu>
      controller_on_gpu_;

  // Accessed on the compositor thread.
  scoped_refptr<gpu::SharedImageInterfaceInProcess> shared_image_interface_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_COMPOSITOR_MEMORY_AND_TASK_CONTROLLER_H_
