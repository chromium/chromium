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
#include "components/viz/service/display_embedder/in_process_gpu_memory_buffer_manager.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image_interface_in_process.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"

namespace viz {

class GmbVideoFramePoolContext
    : public media::RenderableGpuMemoryBufferVideoFramePool::Context,
      public gpu::SharedContextState::ContextLostObserver {
 public:
  explicit GmbVideoFramePoolContext(
      GpuServiceImpl* gpu_service,
      InProcessGpuMemoryBufferManager* gpu_memory_buffer_manager,
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
      base::OnceClosure on_context_lost)
      : gpu_service_(gpu_service),
        gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
        gpu_memory_buffer_factory_(gpu_memory_buffer_factory),
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
        {});

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
        {});

    event.Wait();

    sequence_ = nullptr;
  }

  // Allocate a GpuMemoryBuffer.
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    return gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
        size, format, usage, gpu::kNullSurfaceHandle, nullptr);
  }

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      const SharedImageFormat& si_format,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      gpu::SyncToken& sync_token) override {
    auto client_shared_image = sii_in_process_->CreateSharedImage(
        {si_format, gpu_memory_buffer->GetSize(), color_space, surface_origin,
         alpha_type, usage, "VizGmbVideoFramePool"},
        gpu_memory_buffer->CloneHandle());
    CHECK(client_shared_image);
    sync_token = sii_in_process_->GenVerifiedSyncToken();
    return client_shared_image;
  }

  // Note that currently SharedImageInterface provides 2 different ways to
  // clients to create a MappableSI, one without using existing GMB handle and
  // other using existing GMB handle. The difference being that when a
  // MappableSI is created without clients providing a GMB handle, the shared
  // image created is truly mappable to the CPU memory. Whereas in other case,
  // when a MappableSI is created from an existing handle, it might end up not
  // being CPU mappable, for eg, on Android. That is fine and is actually a
  // requirement in many cases today where clients never Map() the underlying
  // buffer to CPU memory and just uses the underlying external or internal GMB
  // handle to refer to the GPU memory.
  // In order to keep the same behavior as rest of the CreateSharedImage()
  // methods in this class, this method first creates a GMB handle and then
  // creates a shared image from it. Directly creating a MappableSI without
  // providing a GMB handle will end up creating non-native shared memory
  // buffers internally on windows compared to the native buffers created by
  // other CreatedSharedImage methods in this class.
  // TODO(crbug.com/353732994): Refactor this code to not use
  // GpuMemoryBufferFactory for creating GpuMemoryBufferHandles directly.
  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gfx::Size& size,
      gfx::BufferUsage buffer_usage,
      const SharedImageFormat& si_format,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      gpu::SyncToken& sync_token) override {
    // Create a native GMB handle first.
    gfx::GpuMemoryBufferHandle buffer_handle =
        gpu_memory_buffer_factory_->CreateNativeGmbHandle(
            gpu::MappableSIClientGmbId::kGmbVideoFramePoolContext, size,
            gpu::ToBufferFormat(si_format), buffer_usage);
    if (buffer_handle.is_null()) {
      return nullptr;
    }

    // Create a MappableSI from the |buffer_handle|.
    auto client_shared_image = sii_in_process_->CreateSharedImage(
        {si_format, size, color_space, surface_origin, alpha_type, usage,
         "VizGmbVideoFramePool"},
        gpu::kNullSurfaceHandle, buffer_usage, std::move(buffer_handle));
    if (!client_shared_image) {
      return nullptr;
    }
#if BUILDFLAG(IS_MAC)
    client_shared_image->SetColorSpaceOnNativeBuffer(color_space);
#endif
    sync_token = sii_in_process_->GenVerifiedSyncToken();
    return client_shared_image;
  }

  // Destroy a SharedImage created by this interface.
  void DestroySharedImage(const gpu::SyncToken& sync_token,
                          scoped_refptr<gpu::ClientSharedImage> shared_image,
                          const bool is_mappable_si_enabled) override {
    CHECK(shared_image);
    if (is_mappable_si_enabled) {
      shared_image->UpdateDestructionSyncToken(sync_token);
    } else {
      sii_in_process_->DestroySharedImage(sync_token, std::move(shared_image));
    }
  }

 private:
  void InitializeOnGpu(base::WaitableEvent* event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
    DCHECK(!initialized_);
    DCHECK(gpu_service_);

    shared_context_state_ = gpu_service_->GetContextState();
    DCHECK(shared_context_state_);

    shared_context_state_->AddContextLostObserver(this);

    // TODO(bialpio): Move construction to the viz thread once it is no longer
    // necessary to dereference `shared_context_state_` to grab the memory
    // tracker from it.
    sii_in_process_ = base::MakeRefCounted<gpu::SharedImageInterfaceInProcess>(
        sequence_.get(), gpu_service_->sync_point_manager(),
        gpu_service_->gpu_preferences(),
        gpu_service_->gpu_driver_bug_workarounds(),
        gpu_service_->gpu_feature_info(), shared_context_state_.get(),
        gpu_service_->shared_image_manager(),
        /*is_for_display_compositor=*/false);
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
  const raw_ptr<InProcessGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  const raw_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;

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
    GpuServiceImpl* gpu_service,
    InProcessGpuMemoryBufferManager* gpu_memory_buffer_manager,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory)
    : gpu_service_(gpu_service),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory) {}

GmbVideoFramePoolContextProviderImpl::~GmbVideoFramePoolContextProviderImpl() =
    default;

std::unique_ptr<media::RenderableGpuMemoryBufferVideoFramePool::Context>
GmbVideoFramePoolContextProviderImpl::CreateContext(
    base::OnceClosure on_context_lost) {
  return std::make_unique<GmbVideoFramePoolContext>(
      gpu_service_, gpu_memory_buffer_manager_, gpu_memory_buffer_factory_,
      std::move(on_context_lost));
}

}  // namespace viz
