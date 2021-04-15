// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_surface_provider_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"
#include "components/viz/service/display_embedder/gl_output_surface.h"
#include "components/viz/service/display_embedder/gl_output_surface_buffer_queue.h"
#include "components/viz/service/display_embedder/gl_output_surface_offscreen.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/display_embedder/software_output_surface.h"
#include "components/viz/service/display_embedder/viz_process_context_provider.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager_factory.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/command_buffer_task_executor.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/scheduler_sequence.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include "components/viz/service/display_embedder/software_output_device_win.h"
#endif

#if defined(OS_ANDROID)
#include "components/viz/service/display_embedder/gl_output_surface_android.h"
#endif

#if defined(OS_APPLE)
#include "components/viz/service/display_embedder/software_output_device_mac.h"
#include "ui/base/cocoa/remote_layer_api.h"
#endif

#if defined(USE_X11)
#include "components/viz/service/display_embedder/software_output_device_x11.h"
#endif

#if defined(USE_OZONE)
#include "components/viz/service/display_embedder/software_output_device_ozone.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/viz/service/display_embedder/gl_output_surface_chromeos.h"
#include "components/viz/service/display_embedder/output_surface_unified.h"
#endif

namespace viz {

OutputSurfaceProviderImpl::OutputSurfaceProviderImpl(
    GpuServiceImpl* gpu_service_impl,
    gpu::CommandBufferTaskExecutor* task_executor,
    gpu::GpuChannelManagerDelegate* gpu_channel_manager_delegate,
    std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory,
    bool headless)
    : gpu_service_impl_(gpu_service_impl),
      task_executor_(task_executor),
      gpu_channel_manager_delegate_(gpu_channel_manager_delegate),
      gpu_memory_buffer_manager_(std::move(gpu_memory_buffer_manager)),
      image_factory_(image_factory),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      headless_(headless) {}

OutputSurfaceProviderImpl::OutputSurfaceProviderImpl(bool headless)
    : OutputSurfaceProviderImpl(
          /*gpu_service_impl=*/nullptr,
          /*task_executor=*/nullptr,
          /*gpu_channel_manager_delegate=*/nullptr,
          /*gpu_memory_buffer_manager=*/nullptr,
          /*image_factory=*/nullptr,
          headless) {}

OutputSurfaceProviderImpl::~OutputSurfaceProviderImpl() = default;

std::unique_ptr<DisplayCompositorMemoryAndTaskController>
OutputSurfaceProviderImpl::CreateGpuDependency(
    bool gpu_compositing,
    gpu::SurfaceHandle surface_handle,
    const RendererSettings& renderer_settings) {
  if (!gpu_compositing)
    return nullptr;

  if (renderer_settings.use_skia_renderer) {
    gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;
    auto skia_deps = std::make_unique<SkiaOutputSurfaceDependencyImpl>(
        gpu_service_impl_, surface_handle);
    return std::make_unique<DisplayCompositorMemoryAndTaskController>(
        std::move(skia_deps));
  } else {
    DCHECK(task_executor_);
    gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;
    return std::make_unique<DisplayCompositorMemoryAndTaskController>(
        task_executor_, image_factory_);
  }
}

std::unique_ptr<OutputSurface> OutputSurfaceProviderImpl::CreateOutputSurface(
    gpu::SurfaceHandle surface_handle,
    bool gpu_compositing,
    mojom::DisplayClient* display_client,
    DisplayCompositorMemoryAndTaskController* gpu_dependency,
    const RendererSettings& renderer_settings,
    const DebugRendererSettings* debug_settings) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (surface_handle == gpu::kNullSurfaceHandle)
    return std::make_unique<OutputSurfaceUnified>();
#endif

  // TODO(penghuang): Merge two output surfaces into one when GLRenderer and
  // software compositor is removed.
  std::unique_ptr<OutputSurface> output_surface;

  if (!gpu_compositing) {
    output_surface = std::make_unique<SoftwareOutputSurface>(
        CreateSoftwareOutputDeviceForPlatform(surface_handle, display_client));
  } else if (renderer_settings.use_skia_renderer) {
    DCHECK(gpu_dependency);
    {
      gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;
      output_surface = SkiaOutputSurfaceImpl::Create(
          gpu_dependency, renderer_settings, debug_settings);
    }
    if (!output_surface) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMECAST)
      // GPU compositing is expected to always work on Chrome OS so we should
      // never encounter fatal context error. This could be an unrecoverable
      // hardware error or a bug.
      LOG(FATAL) << "Unexpected fatal context error";
#elif !defined(OS_ANDROID)
      gpu_service_impl_->DisableGpuCompositing();
#endif
      return nullptr;
    }
  } else {
    DCHECK(task_executor_);
    DCHECK(gpu_dependency);

    scoped_refptr<VizProcessContextProvider> context_provider;

    // Retry creating and binding |context_provider| on transient failures.
    gpu::ContextResult context_result = gpu::ContextResult::kTransientFailure;
    while (context_result != gpu::ContextResult::kSuccess) {
      // We are about to exit the GPU process so don't try to create a context.
      // It will be recreated after the GPU process restarts. The same check
      // also happens on the GPU thread before the context gets initialized
      // there. If GPU process starts to exit after this check but before
      // context initialization we'll encounter a transient error, loop and hit
      // this check again.
      if (gpu_channel_manager_delegate_->IsExiting())
        return nullptr;

      context_provider = base::MakeRefCounted<VizProcessContextProvider>(
          task_executor_, surface_handle, gpu_memory_buffer_manager_.get(),
          image_factory_, gpu_channel_manager_delegate_, gpu_dependency,
          renderer_settings);
      context_result = context_provider->BindToCurrentThread();

#if defined(OS_ANDROID)
      display_client->OnContextCreationResult(context_result);
#endif

      if (IsFatalOrSurfaceFailure(context_result)) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMECAST)
        // GL compositing is expected to always work on Chrome OS so we should
        // never encounter fatal context error. This could be an unrecoverable
        // hardware error or a bug.
        LOG(FATAL) << "Unexpected fatal context error";
#elif !defined(OS_ANDROID)
        gpu_service_impl_->DisableGpuCompositing();
#endif
        return nullptr;
      }
    }

    if (surface_handle == gpu::kNullSurfaceHandle) {
      output_surface = std::make_unique<GLOutputSurfaceOffscreen>(
          std::move(context_provider));
    } else if (context_provider->ContextCapabilities().surfaceless) {
#if defined(USE_OZONE) || defined(OS_APPLE) || defined(OS_ANDROID)
#if defined(USE_OZONE)
      if (!features::IsUsingOzonePlatform())
        NOTREACHED();
#endif
      output_surface = std::make_unique<GLOutputSurfaceBufferQueue>(
          std::move(context_provider), surface_handle,
          std::make_unique<BufferQueue>(
              context_provider->SharedImageInterface(), surface_handle));
#else
      NOTREACHED();
#endif
    } else {
#if defined(OS_WIN)
      output_surface = std::make_unique<GLOutputSurface>(
          std::move(context_provider), surface_handle);
#elif defined(OS_ANDROID)
      output_surface = std::make_unique<GLOutputSurfaceAndroid>(
          std::move(context_provider), surface_handle);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
      output_surface = std::make_unique<GLOutputSurfaceChromeOS>(
          std::move(context_provider), surface_handle);
#else
      output_surface = std::make_unique<GLOutputSurface>(
          std::move(context_provider), surface_handle);
#endif
    }
  }

  return output_surface;
}

std::unique_ptr<SoftwareOutputDevice>
OutputSurfaceProviderImpl::CreateSoftwareOutputDeviceForPlatform(
    gpu::SurfaceHandle surface_handle,
    mojom::DisplayClient* display_client) {
  if (headless_)
    return std::make_unique<SoftwareOutputDevice>();

#if defined(OS_WIN)
  return CreateSoftwareOutputDeviceWin(surface_handle, &output_device_backing_,
                                       display_client);
#elif defined(OS_APPLE)
  return std::make_unique<SoftwareOutputDeviceMac>(task_runner_);
#elif defined(OS_ANDROID)
  // Android does not do software compositing, so we can't get here.
  NOTREACHED();
  return nullptr;
#elif defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    ui::SurfaceFactoryOzone* factory =
        ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
    std::unique_ptr<ui::PlatformWindowSurface> platform_window_surface =
        factory->CreatePlatformWindowSurface(surface_handle);
    std::unique_ptr<ui::SurfaceOzoneCanvas> surface_ozone =
        factory->CreateCanvasForWidget(surface_handle);
    CHECK(surface_ozone);
    return std::make_unique<SoftwareOutputDeviceOzone>(
        std::move(platform_window_surface), std::move(surface_ozone));
  }
#endif

#if defined(USE_X11)
  return std::make_unique<SoftwareOutputDeviceX11>(surface_handle);
#else
  NOTREACHED();
  return nullptr;
#endif
}

}  // namespace viz
