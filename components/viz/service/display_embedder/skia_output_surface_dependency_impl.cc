// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/service/command_buffer_task_executor.h"
#include "gpu/command_buffer/service/gpu_task_scheduler_helper.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace viz {

SkiaOutputSurfaceDependencyImpl::SkiaOutputSurfaceDependencyImpl(
    GpuServiceImpl* gpu_service_impl,
    gpu::SurfaceHandle surface_handle)
    : gpu_service_impl_(gpu_service_impl),
      surface_handle_(surface_handle),
      client_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()) {}

SkiaOutputSurfaceDependencyImpl::~SkiaOutputSurfaceDependencyImpl() = default;

std::unique_ptr<gpu::SingleTaskSequence>
SkiaOutputSurfaceDependencyImpl::CreateSequence() {
  return std::make_unique<gpu::SchedulerSequence>(
      gpu_service_impl_->GetGpuScheduler(),
      gpu_service_impl_->compositor_gpu_task_runner());
}

gpu::SharedImageManager*
SkiaOutputSurfaceDependencyImpl::GetSharedImageManager() {
  return gpu_service_impl_->shared_image_manager();
}

gpu::SyncPointManager* SkiaOutputSurfaceDependencyImpl::GetSyncPointManager() {
  return gpu_service_impl_->sync_point_manager();
}

const gpu::GpuDriverBugWorkarounds&
SkiaOutputSurfaceDependencyImpl::GetGpuDriverBugWorkarounds() {
  return gpu_service_impl_->gpu_driver_bug_workarounds();
}

scoped_refptr<gpu::SharedContextState>
SkiaOutputSurfaceDependencyImpl::GetSharedContextState() {
  if (gpu_service_impl_->compositor_gpu_thread()) {
    return gpu_service_impl_->compositor_gpu_thread()->GetSharedContextState();
  }
  return gpu_service_impl_->GetContextState();
}

gpu::raster::GrShaderCache*
SkiaOutputSurfaceDependencyImpl::GetGrShaderCache() {
  return gpu_service_impl_->gr_shader_cache();
}

VulkanContextProvider*
SkiaOutputSurfaceDependencyImpl::GetVulkanContextProvider() {
  if (gpu_service_impl_->compositor_gpu_thread()) {
    return gpu_service_impl_->compositor_gpu_thread()
        ->vulkan_context_provider();
  }
  return gpu_service_impl_->vulkan_context_provider();
}

DawnContextProvider* SkiaOutputSurfaceDependencyImpl::GetDawnContextProvider() {
  return gpu_service_impl_->dawn_context_provider();
}

const gpu::GpuPreferences& SkiaOutputSurfaceDependencyImpl::GetGpuPreferences()
    const {
  return gpu_service_impl_->gpu_preferences();
}

const gpu::GpuFeatureInfo&
SkiaOutputSurfaceDependencyImpl::GetGpuFeatureInfo() {
  return gpu_service_impl_->gpu_feature_info();
}

gpu::MailboxManager* SkiaOutputSurfaceDependencyImpl::GetMailboxManager() {
  return gpu_service_impl_->mailbox_manager();
}

gpu::ImageFactory* SkiaOutputSurfaceDependencyImpl::GetGpuImageFactory() {
  return gpu_service_impl_->gpu_image_factory();
}

bool SkiaOutputSurfaceDependencyImpl::IsOffscreen() {
  return surface_handle_ == gpu::kNullSurfaceHandle;
}

gpu::SurfaceHandle SkiaOutputSurfaceDependencyImpl::GetSurfaceHandle() {
  return surface_handle_;
}

scoped_refptr<gl::Presenter> SkiaOutputSurfaceDependencyImpl::CreatePresenter(
    base::WeakPtr<gpu::ImageTransportSurfaceDelegate> stub,
    gl::GLSurfaceFormat format) {
  DCHECK(!IsOffscreen());

  return gpu::ImageTransportSurface::CreatePresenter(
      GetSharedContextState()->display(), stub, surface_handle_, format);
}

scoped_refptr<gl::GLSurface> SkiaOutputSurfaceDependencyImpl::CreateGLSurface(
    base::WeakPtr<gpu::ImageTransportSurfaceDelegate> stub,
    gl::GLSurfaceFormat format) {
  if (IsOffscreen()) {
    return gl::init::CreateOffscreenGLSurfaceWithFormat(
        GetSharedContextState()->display(), gfx::Size(), format);
  } else {
    return gpu::ImageTransportSurface::CreateNativeGLSurface(
        GetSharedContextState()->display(), stub, surface_handle_, format);
  }
}

base::ScopedClosureRunner SkiaOutputSurfaceDependencyImpl::CachePresenter(
    gl::Presenter* presenter) {
  // We're running on the viz thread here. We want to release ref on the
  // compositor gpu thread because presenters are generally not thread-safe. For
  // the same reason we don't want to mark them as RefCountedThreadSafe to avoid
  // confusion and so have to AddRef() on the compositor gpu thread too. It's
  // safe to just PostTask here because SkiaOutputSurfaceImplOnGpu keeps ref on
  // its Presenter and can be only destroyed by PostTask from viz thread to gpu
  // thread which will run after this one.
  gpu_service_impl_->compositor_gpu_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&gl::Presenter::AddRef, base::Unretained(presenter)));

  auto release_callback = base::BindPostTask(
      gpu_service_impl_->compositor_gpu_task_runner(),
      base::BindOnce(&gl::Presenter::Release, base::Unretained(presenter)));

  return base::ScopedClosureRunner(std::move(release_callback));
}

base::ScopedClosureRunner SkiaOutputSurfaceDependencyImpl::CacheGLSurface(
    gl::GLSurface* surface) {
  // We're running on the viz thread here. We want to release ref on the
  // compositor gpu thread because presenters are generally not thread-safe. For
  // the same reason we don't want to mark them as RefCountedThreadSafe to avoid
  // confusion and so have to AddRef() on the compositor gpu thread too. It's
  // safe to just PostTask here because SkiaOutputSurfaceImplOnGpu keeps ref on
  // its GLSurface and can be only destroyed by PostTask from viz thread to gpu
  // thread which will run after this one.
  gpu_service_impl_->compositor_gpu_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&gl::GLSurface::AddRef, base::Unretained(surface)));

  auto release_callback = base::BindPostTask(
      gpu_service_impl_->compositor_gpu_task_runner(),
      base::BindOnce(&gl::GLSurface::Release, base::Unretained(surface)));

  return base::ScopedClosureRunner(std::move(release_callback));
}

scoped_refptr<base::TaskRunner>
SkiaOutputSurfaceDependencyImpl::GetClientTaskRunner() {
  return client_thread_task_runner_;
}

void SkiaOutputSurfaceDependencyImpl::ScheduleGrContextCleanup() {
  GetSharedContextState()->ScheduleGrContextCleanup();
}

void SkiaOutputSurfaceDependencyImpl::ScheduleDelayedGPUTaskFromGPUThread(
    base::OnceClosure task) {
  constexpr base::TimeDelta kDelayForDelayedWork = base::Milliseconds(2);
  gpu_service_impl_->compositor_gpu_task_runner()->PostDelayedTask(
      FROM_HERE, std::move(task), kDelayForDelayedWork);
}

void SkiaOutputSurfaceDependencyImpl::DidLoseContext(
    gpu::error::ContextLostReason reason,
    const GURL& active_url) {
  // |offscreen| is used to determine if it's compositing context or not to
  // decide if we need to disable webgl and canvas.
  gpu_service_impl_->DidLoseContext(/*offscreen=*/false, reason, active_url);
}

base::TimeDelta
SkiaOutputSurfaceDependencyImpl::GetGpuBlockedTimeSinceLastSwap() {
  return gpu_service_impl_->GetGpuScheduler()->TakeTotalBlockingTime();
}

bool SkiaOutputSurfaceDependencyImpl::NeedsSupportForExternalStencil() {
  return false;
}

bool SkiaOutputSurfaceDependencyImpl::IsUsingCompositorGpuThread() {
  return !!gpu_service_impl_->compositor_gpu_thread();
}

}  // namespace viz
