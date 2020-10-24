// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_COMPOSITOR_MEMORY_AND_TASK_CONTROLLER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_COMPOSITOR_MEMORY_AND_TASK_CONTROLLER_H_

#include <memory>

#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/display_compositor_memory_and_task_controller_on_gpu.h"
#include "gpu/ipc/gpu_task_scheduler_helper.h"

namespace base {
class WaitableEvent;
}

namespace gpu {
class ImageFactory;
}

namespace viz {
class GpuServiceImpl;

// This class holds onwership of task posting sequence to the gpu thread and
// memory tracking for the display compositor. This class has a 1:1 relationship
// to the display compositor class. This class is only used for gpu compositing.
class VIZ_SERVICE_EXPORT DisplayCompositorMemoryAndTaskController {
 public:
  // For SkiaRenderer.
  explicit DisplayCompositorMemoryAndTaskController(
      GpuServiceImpl* gpu_service_impl);
  // For WebView SkiaRenderer.
  // TODO(weiliangc): Take in a SkiaOutputSurfaceDependency to avoid difference
  // between WebView and non-WebView SkiaRenderer.
  explicit DisplayCompositorMemoryAndTaskController(
      std::unique_ptr<gpu::SingleTaskSequence> task_sequence);
  // For VizProcessContextProvider that uses InProcessCommandBuffer.
  DisplayCompositorMemoryAndTaskController(
      gpu::CommandBufferTaskExecutor* task_executor,
      gpu::ImageFactory* image_factory);
  DisplayCompositorMemoryAndTaskController(
      const DisplayCompositorMemoryAndTaskController&) = delete;
  DisplayCompositorMemoryAndTaskController& operator=(
      const DisplayCompositorMemoryAndTaskController&) = delete;
  ~DisplayCompositorMemoryAndTaskController();

  gpu::GpuTaskSchedulerHelper* get_gpu_task_scheduler() {
    return gpu_task_scheduler_.get();
  }

  gpu::DisplayCompositorMemoryAndTaskControllerOnGpu* get_controller_on_gpu() {
    return controller_on_gpu_.get();
  }

 private:
  void InitializeOnGpuSkia(GpuServiceImpl* gpu_service_impl,
                           base::WaitableEvent* event);
  void InitializeOnGpuSkiaWebView(base::WaitableEvent* event);
  void InitializeOnGpuGL(gpu::CommandBufferTaskExecutor* task_executor,
                         base::WaitableEvent* event);
  void DestroyOnGpu(base::WaitableEvent* event);

  // Accessed on viz compositor thread.
  std::unique_ptr<gpu::GpuTaskSchedulerHelper> gpu_task_scheduler_;

  // Accessed on the gpu thread.
  std::unique_ptr<gpu::DisplayCompositorMemoryAndTaskControllerOnGpu>
      controller_on_gpu_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_COMPOSITOR_MEMORY_AND_TASK_CONTROLLER_H_
