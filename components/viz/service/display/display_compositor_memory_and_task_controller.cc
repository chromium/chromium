// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/ipc/scheduler_sequence.h"

namespace viz {

DisplayCompositorMemoryAndTaskController::
    DisplayCompositorMemoryAndTaskController(GpuServiceImpl* gpu_service_impl)
    : gpu_task_scheduler_(std::make_unique<gpu::GpuTaskSchedulerHelper>(
          gpu_service_impl->GetGpuScheduler())) {
  DCHECK(gpu_task_scheduler_);
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto callback = base::BindOnce(
      &DisplayCompositorMemoryAndTaskController::InitializeOnGpuSkia,
      base::Unretained(this), gpu_service_impl, &event);
  gpu_task_scheduler_->ScheduleGpuTask(std::move(callback), {});
  event.Wait();
}

DisplayCompositorMemoryAndTaskController::
    DisplayCompositorMemoryAndTaskController(
        std::unique_ptr<gpu::SingleTaskSequence> task_sequence)
    : gpu_task_scheduler_(std::make_unique<gpu::GpuTaskSchedulerHelper>(
          std::move(task_sequence))) {
  DCHECK(gpu_task_scheduler_);
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto callback = base::BindOnce(
      &DisplayCompositorMemoryAndTaskController::InitializeOnGpuSkiaWebView,
      base::Unretained(this), &event);
  gpu_task_scheduler_->ScheduleGpuTask(std::move(callback), {});
  event.Wait();
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
      base::Unretained(this), task_executor, &event);
  gpu_task_scheduler_->GetTaskSequence()->ScheduleTask(std::move(callback), {});
  event.Wait();

  // TODO(weiliangc): Create SharedImageInterface from input params.
}

DisplayCompositorMemoryAndTaskController::
    ~DisplayCompositorMemoryAndTaskController() {
  gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;
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
    GpuServiceImpl* gpu_service_impl,
    base::WaitableEvent* event) {
  DCHECK(event);
  controller_on_gpu_ =
      std::make_unique<gpu::DisplayCompositorMemoryAndTaskControllerOnGpu>(
          gpu_service_impl->GetContextState());
  event->Signal();
}

void DisplayCompositorMemoryAndTaskController::InitializeOnGpuSkiaWebView(
    base::WaitableEvent* event) {
  DCHECK(event);
  controller_on_gpu_ =
      std::make_unique<gpu::DisplayCompositorMemoryAndTaskControllerOnGpu>();
  event->Signal();
}

void DisplayCompositorMemoryAndTaskController::InitializeOnGpuGL(
    gpu::CommandBufferTaskExecutor* task_executor,
    base::WaitableEvent* event) {
  DCHECK(event);
  controller_on_gpu_ =
      std::make_unique<gpu::DisplayCompositorMemoryAndTaskControllerOnGpu>(
          task_executor);
  event->Signal();
}

void DisplayCompositorMemoryAndTaskController::DestroyOnGpu(
    base::WaitableEvent* event) {
  DCHECK(event);
  controller_on_gpu_.reset();
  event->Signal();
}
}  // namespace viz
