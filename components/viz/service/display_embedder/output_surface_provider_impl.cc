// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_surface_provider_impl.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/display_embedder/software_output_surface.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_WIN)
#include "components/viz/service/display_embedder/software_output_device_win.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "components/viz/service/display_embedder/software_output_device_mac.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/base/cocoa/remote_layer_api.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "components/viz/service/display_embedder/software_output_device_ozone.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/viz/service/display_embedder/output_surface_unified.h"
#endif

namespace viz {

OutputSurfaceProviderImpl::OutputSurfaceProviderImpl(
    GpuServiceImpl* gpu_service_impl,
    bool headless)
    : gpu_service_impl_(gpu_service_impl),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      headless_(headless) {}

OutputSurfaceProviderImpl::OutputSurfaceProviderImpl(bool headless)
    : OutputSurfaceProviderImpl(
          /*gpu_service_impl=*/nullptr,
          headless) {}

OutputSurfaceProviderImpl::~OutputSurfaceProviderImpl() = default;

std::unique_ptr<DisplayCompositorMemoryAndTaskController>
OutputSurfaceProviderImpl::CreateGpuDependency(
    bool gpu_compositing,
    gpu::SurfaceHandle surface_handle) {
  if (!gpu_compositing)
    return nullptr;

  gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;
  auto skia_deps = std::make_unique<SkiaOutputSurfaceDependencyImpl>(
      gpu_service_impl_, surface_handle);
  return std::make_unique<DisplayCompositorMemoryAndTaskController>(
      std::move(skia_deps));
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

  if (!gpu_compositing) {
    return std::make_unique<SoftwareOutputSurface>(
        CreateSoftwareOutputDeviceForPlatform(surface_handle, display_client));
  } else {
    DCHECK(gpu_dependency);

    std::unique_ptr<OutputSurface> output_surface;

    {
      gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;
      output_surface = SkiaOutputSurfaceImpl::Create(
          gpu_dependency, renderer_settings, debug_settings);
    }

#if BUILDFLAG(IS_ANDROID)
    // As with non-skia-renderer case, communicate the creation result to
    // CompositorImplAndroid so that it can attempt to recreate the surface on
    // failure.
    display_client->OnContextCreationResult(
        output_surface ? gpu::ContextResult::kSuccess
                       : gpu::ContextResult::kSurfaceFailure);
#endif  // BUILDFLAG(IS_ANDROID)

    if (!output_surface) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CASTOS) || \
    BUILDFLAG(IS_CAST_ANDROID)
      // GPU compositing is expected to always work on Chrome OS and Cast
      // devices, so we should never encounter fatal context error. This could
      // be an unrecoverable hardware error or a bug.
      LOG(FATAL) << "Unexpected fatal context error";
#elif !BUILDFLAG(IS_ANDROID)
      gpu_service_impl_->DisableGpuCompositing();
#endif
    }

    return output_surface;
  }
}

std::unique_ptr<SoftwareOutputDevice>
OutputSurfaceProviderImpl::CreateSoftwareOutputDeviceForPlatform(
    gpu::SurfaceHandle surface_handle,
    mojom::DisplayClient* display_client) {
  if (headless_)
    return std::make_unique<SoftwareOutputDevice>();

#if BUILDFLAG(IS_WIN)
  return CreateSoftwareOutputDeviceWin(surface_handle, &output_device_backing_,
                                       display_client);
#elif BUILDFLAG(IS_APPLE)
  return std::make_unique<SoftwareOutputDeviceMac>(task_runner_);
#elif BUILDFLAG(IS_ANDROID)
  // Android does not do software compositing, so we can't get here.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
#elif BUILDFLAG(IS_OZONE)
  ui::SurfaceFactoryOzone* factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  std::unique_ptr<ui::PlatformWindowSurface> platform_window_surface =
      factory->CreatePlatformWindowSurface(surface_handle);
  std::unique_ptr<ui::SurfaceOzoneCanvas> surface_ozone =
      factory->CreateCanvasForWidget(surface_handle);
  CHECK(surface_ozone);
  return std::make_unique<SoftwareOutputDeviceOzone>(
      std::move(platform_window_surface), std::move(surface_ozone));
#else
  NOTREACHED_IN_MIGRATION();
  return nullptr;
#endif
}

gpu::SharedImageManager* OutputSurfaceProviderImpl::GetSharedImageManager() {
  return gpu_service_impl_->shared_image_manager();
}

gpu::SyncPointManager* OutputSurfaceProviderImpl::GetSyncPointManager() {
  return gpu_service_impl_->sync_point_manager();
}

gpu::Scheduler* OutputSurfaceProviderImpl::GetGpuScheduler() {
  return gpu_service_impl_->gpu_scheduler();
}

}  // namespace viz
