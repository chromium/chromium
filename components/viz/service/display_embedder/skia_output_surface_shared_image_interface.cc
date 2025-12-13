// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_shared_image_interface.h"

#include <cstddef>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/shared_image_interface_in_process_base.h"

namespace viz {

SkiaOutputSurfaceSharedImageInterface::SkiaOutputSurfaceSharedImageInterface(
    SkiaOutputSurfaceImpl& output_surface,
    SkiaOutputSurfaceImplOnGpu& output_surface_on_gpu)
    : gpu::SharedImageInterfaceInProcessBase(
          gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
          output_surface_on_gpu.command_buffer_id(),
          /*verify_creation_sync_token=*/true),
      output_surface_(&output_surface),
      output_surface_on_gpu_(&output_surface_on_gpu),
      host_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

SkiaOutputSurfaceSharedImageInterface::
    ~SkiaOutputSurfaceSharedImageInterface() = default;

void SkiaOutputSurfaceSharedImageInterface::DetachOutputSurfaceOnHostThread() {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());

  if (!output_surface_) {
    return;
  }
  output_surface_->EnqueueGpuTask(
      base::BindOnce(&SkiaOutputSurfaceSharedImageInterface::
                         DetachOutputSurfaceOnGpuThread,
                     this),
      /*sync_tokens=*/{}, /*make_current=*/true, /*need_framebuffer=*/false);
  output_surface_ = nullptr;
}

void SkiaOutputSurfaceSharedImageInterface::DetachOutputSurfaceOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  output_surface_on_gpu_ = nullptr;
}

void SkiaOutputSurfaceSharedImageInterface::ScheduleGpuTask(
    base::OnceClosure task,
    std::vector<gpu::SyncToken> sync_token_fences,
    const gpu::SyncToken& release) {
  // Here we assume that either this is a rendering task scheduled by
  // `SkiaOutputSurfaceImpl`, in which case the output surface will eventually
  // call `FlushGpuTasks()` and signal the `release` token (or one that subsumes
  // it), or the `release` token is empty coming from another task.

  if (host_task_runner_->RunsTasksInCurrentSequence()) {
    ScheduleGpuTaskOnHostThread(std::move(task), std::move(sync_token_fences));
  } else {
    DCHECK(!release.HasData());
    host_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SkiaOutputSurfaceSharedImageInterface::ScheduleGpuTaskOnHostThread,
            this, std::move(task), std::move(sync_token_fences)));
  }
}

void SkiaOutputSurfaceSharedImageInterface::ScheduleGpuTaskOnHostThread(
    base::OnceClosure task,
    std::vector<gpu::SyncToken> sync_token_fences) {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());

  if (!output_surface_) {
    return;
  }
  output_surface_->EnqueueGpuTask(std::move(task), std::move(sync_token_fences),
                                  /*make_current=*/true,
                                  /*need_framebuffer=*/false);
}

gpu::SharedImageFactory*
SkiaOutputSurfaceSharedImageInterface::GetSharedImageFactoryOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (!output_surface_on_gpu_) {
    return nullptr;
  }
  return output_surface_on_gpu_->shared_image_factory();
}

bool SkiaOutputSurfaceSharedImageInterface::MakeContextCurrentOnGpuThread(
    bool needs_gl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  // Passing `make_current=true` in `ScheduleGpuTask()` will call
  // `MakeCurrent()` at an appropriate time so it doesn't need to be done here.
  return output_surface_on_gpu_;
}

void SkiaOutputSurfaceSharedImageInterface::MarkContextLostOnGpuThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (output_surface_on_gpu_) {
    output_surface_on_gpu_->OnContextLost();
  }
}

}  // namespace viz
