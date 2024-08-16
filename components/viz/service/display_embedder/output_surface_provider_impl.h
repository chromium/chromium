// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_SURFACE_PROVIDER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_SURFACE_PROVIDER_IMPL_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/display_embedder/output_surface_provider.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"

#if BUILDFLAG(IS_WIN)
#include "components/viz/service/display_embedder/output_device_backing.h"
#endif

namespace gpu {
class Scheduler;
}  // namespace gpu

namespace viz {
class GpuServiceImpl;
class SoftwareOutputDevice;

// In-process implementation of OutputSurfaceProvider.
class VIZ_SERVICE_EXPORT OutputSurfaceProviderImpl
    : public OutputSurfaceProvider {
 public:
  OutputSurfaceProviderImpl(GpuServiceImpl* gpu_service_impl, bool headless);
  // Software compositing only.
  explicit OutputSurfaceProviderImpl(bool headless);

  OutputSurfaceProviderImpl(const OutputSurfaceProviderImpl&) = delete;
  OutputSurfaceProviderImpl& operator=(const OutputSurfaceProviderImpl&) =
      delete;

  ~OutputSurfaceProviderImpl() override;

  std::unique_ptr<DisplayCompositorMemoryAndTaskController> CreateGpuDependency(
      bool gpu_compositing,
      gpu::SurfaceHandle surface_handle) override;

  // OutputSurfaceProvider implementation.
  std::unique_ptr<OutputSurface> CreateOutputSurface(
      gpu::SurfaceHandle surface_handle,
      bool gpu_compositing,
      mojom::DisplayClient* display_client,
      DisplayCompositorMemoryAndTaskController* gpu_dependency,
      const RendererSettings& renderer_settings,
      const DebugRendererSettings* debug_settings) override;

  gpu::SharedImageManager* GetSharedImageManager() override;
  gpu::SyncPointManager* GetSyncPointManager() override;
  gpu::Scheduler* GetGpuScheduler() override;

 private:
  std::unique_ptr<SoftwareOutputDevice> CreateSoftwareOutputDeviceForPlatform(
      gpu::SurfaceHandle surface_handle,
      mojom::DisplayClient* display_client);

  const raw_ptr<GpuServiceImpl> gpu_service_impl_;

#if BUILDFLAG(IS_WIN)
  // Used for software compositing output on Windows.
  OutputDeviceBacking output_device_backing_;
#endif

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  const bool headless_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_SURFACE_PROVIDER_IMPL_H_
