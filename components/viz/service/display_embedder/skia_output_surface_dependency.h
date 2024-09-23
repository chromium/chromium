// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_DEPENDENCY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_DEPENDENCY_H_

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gl/gl_surface_format.h"

class GURL;

namespace gl {
class GLSurface;
class Presenter;
}

namespace gpu {

class DawnContextProvider;
class GpuDriverBugWorkarounds;
class SharedContextState;
class SharedImageManager;
class SingleTaskSequence;
class SyncPointManager;
struct GpuFeatureInfo;
struct GpuPreferences;

namespace raster {
class GrShaderCache;
}

}  // namespace gpu

namespace viz {

class VulkanContextProvider;

// This class exists to allow SkiaOutputSurfaceImpl to ignore differences
// between Android Webview and regular viz environment.
// Note generally this class only provides access to, but does not own any of
// the objects it returns raw pointers to. Embedders need to ensure these
// objects remain in scope while SkiaOutputSurface is alive. SkiaOutputSurface
// similarly should not hold onto these objects beyond its own lifetime.
class VIZ_SERVICE_EXPORT SkiaOutputSurfaceDependency {
 public:
  virtual ~SkiaOutputSurfaceDependency() = default;

  // Returns a new task execution sequence. Sequences should not outlive the
  // task executor.
  virtual std::unique_ptr<gpu::SingleTaskSequence> CreateSequence() = 0;

  virtual gpu::SharedImageManager* GetSharedImageManager() = 0;
  virtual gpu::SyncPointManager* GetSyncPointManager() = 0;
  virtual const gpu::GpuDriverBugWorkarounds& GetGpuDriverBugWorkarounds() = 0;
  virtual scoped_refptr<gpu::SharedContextState> GetSharedContextState() = 0;
  // May return null.
  virtual gpu::raster::GrShaderCache* GetGrShaderCache() = 0;
  // May return null.
  virtual VulkanContextProvider* GetVulkanContextProvider() = 0;
  // May return null.
  virtual gpu::DawnContextProvider* GetDawnContextProvider() = 0;
  virtual const gpu::GpuPreferences& GetGpuPreferences() const = 0;
  virtual const gpu::GpuFeatureInfo& GetGpuFeatureInfo() = 0;
  // Note it is possible for IsOffscreen to be false and GetSurfaceHandle to
  // return kNullSurfaceHandle.
  virtual bool IsOffscreen() = 0;
  virtual gpu::SurfaceHandle GetSurfaceHandle() = 0;
  virtual scoped_refptr<gl::Presenter> CreatePresenter() = 0;
  virtual scoped_refptr<gl::GLSurface> CreateGLSurface(
      gl::GLSurfaceFormat format) = 0;
#if BUILDFLAG(IS_ANDROID)
  // Hold a ref of the given surface until the returned closure is fired.
  virtual base::ScopedClosureRunner CacheGLSurface(gl::GLSurface* surface) = 0;
  virtual base::ScopedClosureRunner CachePresenter(
      gl::Presenter* presenter) = 0;
#endif
  virtual void ScheduleGrContextCleanup() = 0;

  void PostTaskToClientThread(base::OnceClosure closure) {
    GetClientTaskRunner()->PostTask(FROM_HERE, std::move(closure));
  }
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetClientTaskRunner() = 0;

  // This function schedules delayed task to be run on GPUThread. It can be
  // called only from GPU Thread.
  virtual void ScheduleDelayedGPUTaskFromGPUThread(base::OnceClosure task) = 0;

  virtual void DidLoseContext(gpu::error::ContextLostReason reason,
                              const GURL& active_url) = 0;

  virtual bool NeedsSupportForExternalStencil() = 0;

  // This returns true if CompositorGpuThread(aka DrDc thread) is enabled.
  virtual bool IsUsingCompositorGpuThread() = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_DEPENDENCY_H_
