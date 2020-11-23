// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/ipc/scheduler_sequence.h"
#include "gpu/ipc/shared_image_interface_in_process.h"

namespace viz {

DisplayCompositorMemoryAndTaskController::
    DisplayCompositorMemoryAndTaskController(
        std::unique_ptr<SkiaOutputSurfaceDependency> skia_dependency)
    : skia_dependency_(std::move(skia_dependency)),
      gpu_task_scheduler_(std::make_unique<gpu::GpuTaskSchedulerHelper>(
          skia_dependency_->CreateSequence())) {
  DCHECK(gpu_task_scheduler_);
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto callback = base::BindOnce(
      &DisplayCompositorMemoryAndTaskController::InitializeOnGpuSkia,
      base::Unretained(this), skia_dependency_.get(), &event);
  gpu_task_scheduler_->ScheduleGpuTask(std::move(callback), {});
  event.Wait();

  shared_image_interface_ =
      std::make_unique<gpu::SharedImageInterfaceInProcess>(
          gpu_task_scheduler_->GetTaskSequence(), controller_on_gpu_.get(),
          nullptr /* command_buffer_helper*/);
}

DisplayCompositorMemoryAndTaskController::
    DisplayCompositorMemoryAndTaskController(
        gpu::CommandBufferTaskExecutor* task_executor,
        gpu::ImageFactory* image_factory)
    : gpu_task_scheduler_(
          std::make_unique<gpu::GpuTaskSchedulerHelper>(task_executor)) {
  DCHECK(gpu_task_scheduler_);
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto callback = base::BindOnce(
      &DisplayCompositorMemoryAndTaskController::InitializeOnGpuGL,
      base::Unretained(this), task_executor, image_factory, &event);
  gpu_task_scheduler_->GetTaskSequence()->ScheduleTask(std::move(callback), {});
  event.Wait();

  // TODO(weiliangc): Move VizProcessContextProvider initialization here to take
  // ownership of the shared image interface.
}

DisplayCompositorMemoryAndTaskController::
    ~DisplayCompositorMemoryAndTaskController() {
  gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;
  // Make sure to destroy the SharedImageInterfaceInProcess before getting rid
  // of data structures on the gpu thread.
  shared_image_interface_.reset();

  // If we have a |gpu_task_scheduler_|, we must have started initializing
  // a |controller_on_gpu_| on the |gpu_task_scheduler_|.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto callback =
      base::BindOnce(&DisplayCompositorMemoryAndTaskController::DestroyOnGpu,
                     base::Unretained(this), &event);
  gpu_task_scheduler_->GetTaskSequence()->ScheduleTask(std::move(callback), {});
  event.Wait();
}

void DisplayCompositorMemoryAndTaskController::InitializeOnGpuSkia(
    SkiaOutputSurfaceDependency* skia_dependency,
    base::WaitableEvent* event) {
  DCHECK(event);
  controller_on_gpu_ =
      std::make_unique<gpu::DisplayCompositorMemoryAndTaskControllerOnGpu>(
          skia_dependency->GetSharedContextState(),
          skia_dependency->GetMailboxManager(),
          skia_dependency->GetGpuImageFactory(),
          skia_dependency->GetSharedImageManager(),
          skia_dependency->GetSyncPointManager(),
          skia_dependency->GetGpuPreferences(),
          skia_dependency->GetGpuDriverBugWorkarounds(),
          skia_dependency->GetGpuFeatureInfo());
  event->Signal();
}

void DisplayCompositorMemoryAndTaskController::InitializeOnGpuGL(
    gpu::CommandBufferTaskExecutor* task_executor,
    gpu::ImageFactory* image_factory,
    base::WaitableEvent* event) {
  DCHECK(event);
  controller_on_gpu_ =
      std::make_unique<gpu::DisplayCompositorMemoryAndTaskControllerOnGpu>(
          task_executor, image_factory);
  event->Signal();
}

void DisplayCompositorMemoryAndTaskController::DestroyOnGpu(
    base::WaitableEvent* event) {
  DCHECK(event);
  controller_on_gpu_.reset();
  event->Signal();
}

gpu::SharedImageInterface*
DisplayCompositorMemoryAndTaskController::shared_image_interface() {
  return shared_image_interface_.get();
}
}  // namespace viz
