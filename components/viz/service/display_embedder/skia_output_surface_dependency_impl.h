// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_DEPENDENCY_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_DEPENDENCY_IMPL_H_

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace viz {

class GpuServiceImpl;

class VIZ_SERVICE_EXPORT SkiaOutputSurfaceDependencyImpl
    : public SkiaOutputSurfaceDependency {
 public:
  SkiaOutputSurfaceDependencyImpl(
      GpuServiceImpl* gpu_service_impl,
      gpu::SurfaceHandle surface_handle);

  SkiaOutputSurfaceDependencyImpl(const SkiaOutputSurfaceDependencyImpl&) =
      delete;
  SkiaOutputSurfaceDependencyImpl& operator=(
      const SkiaOutputSurfaceDependencyImpl&) = delete;

  ~SkiaOutputSurfaceDependencyImpl() override;

  std::unique_ptr<gpu::SingleTaskSequence> CreateSequence() override;
  gpu::SharedImageManager* GetSharedImageManager() override;
  gpu::SyncPointManager* GetSyncPointManager() override;
  const gpu::GpuDriverBugWorkarounds& GetGpuDriverBugWorkarounds() override;
  scoped_refptr<gpu::SharedContextState> GetSharedContextState() override;
  gpu::raster::GrShaderCache* GetGrShaderCache() override;
  VulkanContextProvider* GetVulkanContextProvider() override;
  gpu::DawnContextProvider* GetDawnContextProvider() override;
  const gpu::GpuPreferences& GetGpuPreferences() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() override;
  bool IsOffscreen() override;
  gpu::SurfaceHandle GetSurfaceHandle() override;
  scoped_refptr<gl::GLSurface> CreateGLSurface(
      gl::GLSurfaceFormat format) override;
  scoped_refptr<gl::Presenter> CreatePresenter() override;
#if BUILDFLAG(IS_ANDROID)
  base::ScopedClosureRunner CachePresenter(gl::Presenter* presenter) override;
  base::ScopedClosureRunner CacheGLSurface(gl::GLSurface* surface) override;
#endif
  scoped_refptr<base::SingleThreadTaskRunner> GetClientTaskRunner() override;
  void ScheduleGrContextCleanup() override;
  void ScheduleDelayedGPUTaskFromGPUThread(base::OnceClosure task) override;
  void DidLoseContext(gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;

  bool NeedsSupportForExternalStencil() override;
  bool IsUsingCompositorGpuThread() override;

 private:
  const raw_ptr<GpuServiceImpl> gpu_service_impl_;
  const gpu::SurfaceHandle surface_handle_;
  scoped_refptr<base::SingleThreadTaskRunner> client_thread_task_runner_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_DEPENDENCY_IMPL_H_
