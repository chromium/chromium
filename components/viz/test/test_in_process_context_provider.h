// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_TEST_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/config/gpu_feature_info.h"

class GrDirectContext;

namespace gpu {
class GLInProcessContext;
class GpuProcessShmCount;
class RasterInProcessContext;

namespace raster {
class GrShaderCache;
}
}  // namespace gpu

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace viz {
class GpuServiceImpl;

enum TestContextType {
  kGLES2,            // Provides GLES2Interface.
  kSoftwareRaster,   // Provides RasterInterface for software raster.
  kGpuRaster         // Provides RasterInterface for GPU raster.
};

class TestInProcessContextProvider
    : public base::RefCountedThreadSafe<TestInProcessContextProvider>,
      public ContextProvider,
      public RasterContextProvider {
 public:
  explicit TestInProcessContextProvider(
      TestContextType type,
      bool support_locking,
      gpu::raster::GrShaderCache* gr_shader_cache = nullptr,
      gpu::GpuProcessShmCount* use_shader_cache_shm_count = nullptr);

  // ContextProvider / RasterContextProvider implementation.
  void AddRef() const override;
  void Release() const override;
  gpu::ContextResult BindToCurrentSequence() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::raster::RasterInterface* RasterInterface() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrDirectContext* GrContext() override;
  gpu::SharedImageInterface* SharedImageInterface() override;
  ContextCacheController* CacheController() override;
  base::Lock* GetLock() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  void AddObserver(ContextLostObserver* obs) override;
  void RemoveObserver(ContextLostObserver* obs) override;
  unsigned int GetGrGLTextureFormat(SharedImageFormat format) const override;
  GpuServiceImpl* GpuService();

  // Calls OnContextLost() on all observers. This doesn't modify the context.
  void SendOnContextLost();

  void ExecuteOnGpuThread(base::OnceClosure task);

 protected:
  friend class base::RefCountedThreadSafe<TestInProcessContextProvider>;
  ~TestInProcessContextProvider() override;

 private:
  void CheckValidThreadOrLockAcquired() const;

  const TestContextType type_;
  raw_ptr<gpu::raster::GrShaderCache> gr_shader_cache_ = nullptr;
  raw_ptr<gpu::GpuProcessShmCount> use_shader_cache_shm_count_ = nullptr;
  bool is_bound_ = false;

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  gpu::Capabilities caps_;

  // Used for GLES2 contexts only.
  std::unique_ptr<gpu::GLInProcessContext> gles2_context_;
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;

  // Used for raster contexts only.
  std::unique_ptr<gpu::RasterInProcessContext> raster_context_;

  std::unique_ptr<ContextCacheController> cache_controller_;
  std::optional<base::Lock> context_lock_;

  base::ObserverList<ContextLostObserver>::Unchecked observers_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_
