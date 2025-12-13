// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/gmb_video_frame_pool_context_provider_impl.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image_interface_in_process.h"
#include "gpu/ipc/common/gpu_client_ids.h"

namespace viz {

class GmbVideoFramePoolContext
    : public media::RenderableGpuMemoryBufferVideoFramePool::Context,
      public gpu::SharedContextState::ContextLostObserver {
 public:
  explicit GmbVideoFramePoolContext(
      GpuServiceImpl* gpu_service,
      base::OnceClosure on_context_lost)
      : gpu_service_(gpu_service),
        on_context_lost_(
            base::BindPostTaskToCurrentDefault(std::move(on_context_lost))) {
    DETACH_FROM_SEQUENCE(gpu_sequence_checker_);

    // TODO(vikassoni): Verify this is the right GPU thread/sequence for DrDC.
    sequence_ = std::make_unique<gpu::SchedulerSequence>(
        gpu_service_->GetGpuScheduler(), gpu_service_->main_runner(),
        /*target_thread_is_always_available=*/true);

    base::WaitableEvent event;

    sequence_->ScheduleTask(
        base::BindOnce(&GmbVideoFramePoolContext::InitializeOnGpu,
                       base::Unretained(this), &event),
        /*sync_token_fences=*/{}, gpu::SyncToken());

    event.Wait();
  }

  ~GmbVideoFramePoolContext() override {
    // SharedImageInterfaceInProcess' dtor blocks on GPU, which we want to do as
    // well, so run it now before we grab the GPU thread:
    sii_in_process_ = nullptr;

    base::WaitableEvent event;

    sequence_->ScheduleTask(
        base::BindOnce(&GmbVideoFramePoolContext::DestroyOnGpu,
                       base::Unretained(this), &event),
        /*sync_token_fences=*/{}, gpu::SyncToken());

    event.Wait();

    sequence_ = nullptr;
  }

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gfx::Size& size,
      gfx::BufferUsage buffer_usage,
      const SharedImageFormat& format,
      const gfx::ColorSpace& color_space,
      gpu::SharedImageUsageSet usage,
      gpu::SyncToken& sync_token) override {
    auto client_shared_image = sii_in_process_->CreateSharedImage(
        {format, size, color_space, usage, "VizGmbVideoFramePool"},
        gpu::kNullSurfaceHandle, buffer_usage, std::nullopt);
    if (!client_shared_image) {
      return nullptr;
    }
    sync_token = sii_in_process_->GenVerifiedSyncToken();
    return client_shared_image;
  }

  // Destroy a SharedImage created by this interface.
  void DestroySharedImage(
      const gpu::SyncToken& sync_token,
      scoped_refptr<gpu::ClientSharedImage> shared_image) override {
    CHECK(shared_image);
    shared_image->UpdateDestructionSyncToken(sync_token);
  }

  const gpu::SharedImageCapabilities& GetCapabilities() override {
    return sii_in_process_->GetCapabilities();
  }

 private:
  void InitializeOnGpu(base::WaitableEvent* event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
    DCHECK(!initialized_);
    DCHECK(gpu_service_);

    shared_context_state_ = gpu_service_->GetContextState();
    DCHECK(shared_context_state_);

    shared_context_state_->AddContextLostObserver(this);

    // This class historically created native GMB handles directly and then
    // passed them to CreateSharedImage(), bypassing internal SII checks for
    // whether native GMB handles are supported. Preserve that
    // behavior by configuring the SII here to always create native GMB
    // handles. Note that this requires creating the SII on the GPU thread,
    // as it internally needs to create the SharedImageFactory eagerly to
    // ensure that it is available for usage on the IO thread to create native
    // GMB handles in response to CreateSharedImage() calls.
    sii_in_process_ = gpu::SharedImageInterfaceInProcess::Create(
        sequence_.get(), gpu_service_->gpu_preferences(),
        gpu_service_->gpu_driver_bug_workarounds(),
        gpu_service_->gpu_feature_info(), shared_context_state_.get(),
        gpu_service_->shared_image_manager(),
        /*is_for_display_compositor=*/false, gpu_service_->main_runner(),
        /*always_create_native_gmb_handle=*/true);

    DCHECK(sii_in_process_);

    initialized_ = true;

    event->Signal();
  }

  void DestroyOnGpu(base::WaitableEvent* event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
    DCHECK(initialized_);

    shared_context_state_->RemoveContextLostObserver(this);
    shared_context_state_ = nullptr;

    event->Signal();
  }

  // gpu::SharedContextState::ContextLostObserver implementation:
  void OnContextLost() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

    DCHECK(on_context_lost_);
    std::move(on_context_lost_).Run();
  }

  const raw_ptr<GpuServiceImpl> gpu_service_;

  // Closure that we need to call when context loss happens.
  base::OnceClosure on_context_lost_;

  // True iff the context was initialized on GPU.
  bool initialized_ = false;

  std::unique_ptr<gpu::SchedulerSequence> sequence_;
  scoped_refptr<gpu::SharedContextState> shared_context_state_;

  scoped_refptr<gpu::SharedImageInterfaceInProcess> sii_in_process_;

  SEQUENCE_CHECKER(gpu_sequence_checker_);
};

GmbVideoFramePoolContextProviderImpl::GmbVideoFramePoolContextProviderImpl(
    GpuServiceImpl* gpu_service)
    : gpu_service_(gpu_service) {}

GmbVideoFramePoolContextProviderImpl::~GmbVideoFramePoolContextProviderImpl() =
    default;

std::unique_ptr<media::RenderableGpuMemoryBufferVideoFramePool::Context>
GmbVideoFramePoolContextProviderImpl::CreateContext(
    base::OnceClosure on_context_lost) {
  return std::make_unique<GmbVideoFramePoolContext>(gpu_service_,
                                                    std::move(on_context_lost));
}

}  // namespace viz
