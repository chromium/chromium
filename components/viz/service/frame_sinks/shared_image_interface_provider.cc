// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/shared_image_interface_provider.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_interface_in_process.h"

namespace viz {

SharedImageInterfaceProvider::SharedImageInterfaceProvider(
    GpuServiceImpl* gpu_service)
    : gpu_service_(gpu_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
}

SharedImageInterfaceProvider::~SharedImageInterfaceProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  if (shared_context_state_) {
    shared_context_state_->RemoveContextLostObserver(this);
    shared_context_state_.reset();
  }
}

gpu::SharedImageInterface*
SharedImageInterfaceProvider::GetSharedImageInterface() {
  if (NeedsNewSharedImageInterface()) {
    CreateSharedImageInterface();
  }
  return shared_image_interface_.get();
}

bool SharedImageInterfaceProvider::NeedsNewSharedImageInterface() const {
  return !shared_image_interface_ || !shared_context_state_;
}

void SharedImageInterfaceProvider::CreateSharedImageInterface() {
  if (!scheduler_sequence_) {
    // TODO(vmpstr): This can use compositor_gpu_task_runner instead. However,
    // we also then need to create a SharedContextState from the same runner.
    // That checks that the access is happening from the thread that owns the
    // runner, which would not be the case here. All of this, however, is an
    // optimization and for now we can use main runner for these textures.
    scheduler_sequence_ = std::make_unique<gpu::SchedulerSequence>(
        gpu_service_->GetGpuScheduler(), gpu_service_->main_runner(),
        /*target_thread_is_always_available=*/true);
  }

  // This function should only be called on the compositor thread.
  CHECK(!gpu_service_->main_runner()->BelongsToCurrentThread());

  base::WaitableEvent event;
  scheduler_sequence_->ScheduleTask(
      base::BindOnce(
          &SharedImageInterfaceProvider::CreateSharedImageInterfaceOnGpu,
          base::Unretained(this), &event),
      {});
  event.Wait();
}

void SharedImageInterfaceProvider::CreateSharedImageInterfaceOnGpu(
    base::WaitableEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  shared_context_state_ =
      gl::GetGLImplementation() != gl::kGLImplementationDisabled && gpu_service_
          ? gpu_service_->GetContextState()
          : nullptr;

  shared_image_interface_ =
      base::MakeRefCounted<gpu::SharedImageInterfaceInProcess>(
          scheduler_sequence_.get(), gpu_service_->sync_point_manager(),
          gpu_service_->gpu_preferences(),
          gpu_service_->gpu_driver_bug_workarounds(),
          gpu_service_->gpu_feature_info(), shared_context_state_.get(),
          gpu_service_->shared_image_manager(),
          /*is_for_diplay_compositor=*/false,
          gpu::SharedImageInterfaceInProcess::OwnerThread::kGpu);

  if (shared_context_state_) {
    shared_context_state_->AddContextLostObserver(this);
  }

  event->Signal();
}

void SharedImageInterfaceProvider::OnContextLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  shared_context_state_->RemoveContextLostObserver(this);
  shared_context_state_ = nullptr;
}

}  // namespace viz
