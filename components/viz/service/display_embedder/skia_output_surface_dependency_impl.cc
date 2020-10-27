// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/ipc/gpu_task_scheduler_helper.h"
#include "gpu/ipc/scheduler_sequence.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace viz {

SkiaOutputSurfaceDependencyImpl::SkiaOutputSurfaceDependencyImpl(
    GpuServiceImpl* gpu_service_impl,
    gpu::SurfaceHandle surface_handle)
    : gpu_service_impl_(gpu_service_impl),
      surface_handle_(surface_handle),
      client_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

SkiaOutputSurfaceDependencyImpl::~SkiaOutputSurfaceDependencyImpl() = default;

std::unique_ptr<gpu::SingleTaskSequence>
SkiaOutputSurfaceDependencyImpl::CreateSequence() {
  return std::make_unique<gpu::SchedulerSequence>(
      gpu_service_impl_->GetGpuScheduler());
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
  return gpu_service_impl_->gpu_channel_manager()->gpu_driver_bug_workarounds();
}

scoped_refptr<gpu::SharedContextState>
SkiaOutputSurfaceDependencyImpl::GetSharedContextState() {
  return gpu_service_impl_->GetContextState();
}

gpu::raster::GrShaderCache*
SkiaOutputSurfaceDependencyImpl::GetGrShaderCache() {
  return gpu_service_impl_->gr_shader_cache();
}

VulkanContextProvider*
SkiaOutputSurfaceDependencyImpl::GetVulkanContextProvider() {
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

scoped_refptr<gl::GLSurface> SkiaOutputSurfaceDependencyImpl::CreateGLSurface(
    base::WeakPtr<gpu::ImageTransportSurfaceDelegate> stub,
    gl::GLSurfaceFormat format) {
  if (IsOffscreen()) {
    return gl::init::CreateOffscreenGLSurfaceWithFormat(gfx::Size(), format);
  } else {
    return gpu::ImageTransportSurface::CreateNativeSurface(
        stub, surface_handle_, format);
  }
}

base::ScopedClosureRunner SkiaOutputSurfaceDependencyImpl::CacheGLSurface(
    gl::GLSurface* surface) {
  gpu_service_impl_->main_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&gl::GLSurface::AddRef, base::Unretained(surface)));
  auto release_callback = base::BindOnce(
      [](const scoped_refptr<base::SequencedTaskRunner>& runner,
         gl::GLSurface* surface) {
        runner->PostTask(FROM_HERE, base::BindOnce(&gl::GLSurface::Release,
                                                   base::Unretained(surface)));
      },
      gpu_service_impl_->main_runner(), base::Unretained(surface));
  return base::ScopedClosureRunner(std::move(release_callback));
}

void SkiaOutputSurfaceDependencyImpl::PostTaskToClientThread(
    base::OnceClosure closure) {
  client_thread_task_runner_->PostTask(FROM_HERE, std::move(closure));
}

void SkiaOutputSurfaceDependencyImpl::ScheduleGrContextCleanup() {
  gpu_service_impl_->gpu_channel_manager()->ScheduleGrContextCleanup();
}

void SkiaOutputSurfaceDependencyImpl::ScheduleDelayedGPUTaskFromGPUThread(
    base::OnceClosure task) {
  DCHECK(gpu_service_impl_->main_runner()->BelongsToCurrentThread());

  constexpr base::TimeDelta kDelayForDelayedWork =
      base::TimeDelta::FromMilliseconds(2);
  gpu_service_impl_->main_runner()->PostDelayedTask(FROM_HERE, std::move(task),
                                                    kDelayForDelayedWork);
}

#if defined(OS_WIN)
void SkiaOutputSurfaceDependencyImpl::DidCreateAcceleratedSurfaceChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  gpu_service_impl_->SendCreatedChildWindow(parent_window, child_window);
}
#endif

void SkiaOutputSurfaceDependencyImpl::RegisterDisplayContext(
    gpu::DisplayContext* display_context) {
  gpu_service_impl_->RegisterDisplayContext(display_context);
}

void SkiaOutputSurfaceDependencyImpl::UnregisterDisplayContext(
    gpu::DisplayContext* display_context) {
  gpu_service_impl_->UnregisterDisplayContext(display_context);
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

}  // namespace viz
