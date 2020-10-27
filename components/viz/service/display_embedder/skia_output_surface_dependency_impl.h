// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_DEPENDENCY_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_DEPENDENCY_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace viz {

class GpuServiceImpl;

class VIZ_SERVICE_EXPORT SkiaOutputSurfaceDependencyImpl
    : public SkiaOutputSurfaceDependency {
 public:
  SkiaOutputSurfaceDependencyImpl(GpuServiceImpl* gpu_service_impl,
                                  gpu::SurfaceHandle surface_handle);
  ~SkiaOutputSurfaceDependencyImpl() override;

  std::unique_ptr<gpu::SingleTaskSequence> CreateSequence() override;
  gpu::SharedImageManager* GetSharedImageManager() override;
  gpu::SyncPointManager* GetSyncPointManager() override;
  const gpu::GpuDriverBugWorkarounds& GetGpuDriverBugWorkarounds() override;
  scoped_refptr<gpu::SharedContextState> GetSharedContextState() override;
  gpu::raster::GrShaderCache* GetGrShaderCache() override;
  VulkanContextProvider* GetVulkanContextProvider() override;
  DawnContextProvider* GetDawnContextProvider() override;
  const gpu::GpuPreferences& GetGpuPreferences() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() override;
  gpu::MailboxManager* GetMailboxManager() override;
  gpu::ImageFactory* GetGpuImageFactory() override;
  bool IsOffscreen() override;
  gpu::SurfaceHandle GetSurfaceHandle() override;
  scoped_refptr<gl::GLSurface> CreateGLSurface(
      base::WeakPtr<gpu::ImageTransportSurfaceDelegate> stub,
      gl::GLSurfaceFormat format) override;
  base::ScopedClosureRunner CacheGLSurface(gl::GLSurface* surface) override;
  void PostTaskToClientThread(base::OnceClosure closure) override;
  void ScheduleGrContextCleanup() override;
  void ScheduleDelayedGPUTaskFromGPUThread(base::OnceClosure task) override;

#if defined(OS_WIN)
  void DidCreateAcceleratedSurfaceChildWindow(
      gpu::SurfaceHandle parent_window,
      gpu::SurfaceHandle child_window) override;
#endif

  void RegisterDisplayContext(gpu::DisplayContext* display_context) override;
  void UnregisterDisplayContext(gpu::DisplayContext* display_context) override;
  void DidLoseContext(gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;

  base::TimeDelta GetGpuBlockedTimeSinceLastSwap() override;
  bool NeedsSupportForExternalStencil() override;

 private:
  GpuServiceImpl* const gpu_service_impl_;
  const gpu::SurfaceHandle surface_handle_;
  scoped_refptr<base::SingleThreadTaskRunner> client_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurfaceDependencyImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_DEPENDENCY_IMPL_H_
