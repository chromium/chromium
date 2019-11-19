// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_surface_provider_impl.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/switches.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
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

#if defined(OS_MACOSX)
#include "components/viz/service/display_embedder/software_output_device_mac.h"
#include "ui/base/cocoa/remote_layer_api.h"
#endif

#if defined(USE_X11)
#include "components/viz/service/display_embedder/software_output_device_x11.h"
#endif

#if defined(USE_OZONE)
#include "components/viz/service/display_embedder/software_output_device_ozone.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#endif

#if defined(OS_CHROMEOS)
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

std::unique_ptr<OutputSurface> OutputSurfaceProviderImpl::CreateOutputSurface(
    gpu::SurfaceHandle surface_handle,
    bool gpu_compositing,
    mojom::DisplayClient* display_client,
    const RendererSettings& renderer_settings) {
#if defined(OS_CHROMEOS)
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
#if defined(OS_MACOSX)
    // TODO(penghuang): Support SkiaRenderer for all platforms.
    NOTIMPLEMENTED();
    return nullptr;
#else
    {
      gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;
      output_surface = SkiaOutputSurfaceImpl::Create(
          std::make_unique<SkiaOutputSurfaceDependencyImpl>(gpu_service_impl_,
                                                            surface_handle),
          renderer_settings);
    }
    if (!output_surface) {
#if defined(OS_CHROMEOS) || defined(IS_CHROMECAST)
      // GPU compositing is expected to always work on Chrome OS so we should
      // never encounter fatal context error. This could be an unrecoverable
      // hardware error or a bug.
      LOG(FATAL) << "Unexpected fatal context error";
#elif !defined(OS_ANDROID)
      gpu_service_impl_->DisableGpuCompositing();
#endif
      return nullptr;
    }
#endif
  } else {
    DCHECK(task_executor_);

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
          image_factory_, gpu_channel_manager_delegate_, renderer_settings);
      context_result = context_provider->BindToCurrentThread();

#if defined(OS_ANDROID)
      display_client->OnContextCreationResult(context_result);
#endif

      if (IsFatalOrSurfaceFailure(context_result)) {
#if defined(OS_CHROMEOS) || defined(IS_CHROMECAST)
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
#if defined(USE_OZONE)
      output_surface = std::make_unique<GLOutputSurfaceBufferQueue>(
          std::move(context_provider), surface_handle,
          gpu_memory_buffer_manager_.get(),
          display::DisplaySnapshot::PrimaryFormat());
#elif defined(OS_MACOSX)
      output_surface = std::make_unique<GLOutputSurfaceBufferQueue>(
          std::move(context_provider), surface_handle,
          gpu_memory_buffer_manager_.get(), gfx::BufferFormat::RGBA_8888);
#elif defined(OS_ANDROID)
      auto buffer_format = context_provider->UseRGB565PixelFormat()
                               ? gfx::BufferFormat::BGR_565
                               : gfx::BufferFormat::RGBA_8888;
      output_surface = std::make_unique<GLOutputSurfaceBufferQueue>(
          std::move(context_provider), surface_handle,
          gpu_memory_buffer_manager_.get(), buffer_format);
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
#elif defined(OS_CHROMEOS)
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
#elif defined(OS_MACOSX)
  return std::make_unique<SoftwareOutputDeviceMac>(task_runner_);
#elif defined(OS_ANDROID)
  // Android does not do software compositing, so we can't get here.
  NOTREACHED();
  return nullptr;
#elif defined(USE_OZONE)
  ui::SurfaceFactoryOzone* factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  std::unique_ptr<ui::PlatformWindowSurface> platform_window_surface =
      factory->CreatePlatformWindowSurface(surface_handle);
  std::unique_ptr<ui::SurfaceOzoneCanvas> surface_ozone =
      factory->CreateCanvasForWidget(surface_handle,
                                     gpu_service_impl_->in_host_process()
                                         ? nullptr
                                         : gpu_service_impl_->main_runner());
  CHECK(surface_ozone);
  return std::make_unique<SoftwareOutputDeviceOzone>(
      std::move(platform_window_surface), std::move(surface_ozone));
#elif defined(USE_X11)
  return std::make_unique<SoftwareOutputDeviceX11>(
      surface_handle, gpu_service_impl_->in_host_process()
                          ? nullptr
                          : gpu_service_impl_->main_runner());
#endif
}

}  // namespace viz
