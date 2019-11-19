// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_TEST_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "components/viz/test/test_image_factory.h"
#include "gpu/config/gpu_feature_info.h"

class GrContext;

namespace gpu {
class GLInProcessContext;
class GpuProcessActivityFlags;
class RasterInProcessContext;

namespace raster {
class GrShaderCache;
}
}  // namespace gpu

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace viz {

std::unique_ptr<gpu::GLInProcessContext> CreateTestInProcessContext();

class TestInProcessContextProvider
    : public base::RefCountedThreadSafe<TestInProcessContextProvider>,
      public ContextProvider,
      public RasterContextProvider {
 public:
  explicit TestInProcessContextProvider(
      bool enable_oop_rasterization,
      bool support_locking,
      gpu::raster::GrShaderCache* gr_shader_cache = nullptr,
      gpu::GpuProcessActivityFlags* activity_flags = nullptr);

  // ContextProvider / RasterContextProvider implementation.
  void AddRef() const override;
  void Release() const override;
  gpu::ContextResult BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::raster::RasterInterface* RasterInterface() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  gpu::SharedImageInterface* SharedImageInterface() override;
  ContextCacheController* CacheController() override;
  base::Lock* GetLock() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  void AddObserver(ContextLostObserver* obs) override {}
  void RemoveObserver(ContextLostObserver* obs) override {}

  void ExecuteOnGpuThread(base::OnceClosure task);

 protected:
  friend class base::RefCountedThreadSafe<TestInProcessContextProvider>;
  ~TestInProcessContextProvider() override;

 private:
  bool enable_oop_rasterization_ = false;
  gpu::raster::GrShaderCache* gr_shader_cache_ = nullptr;
  gpu::GpuProcessActivityFlags* activity_flags_ = nullptr;

  TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  TestImageFactory image_factory_;

  // Used if support_gles2_interface.
  std::unique_ptr<gpu::GLInProcessContext> gles2_context_;
  std::unique_ptr<gpu::raster::RasterInterface> raster_implementation_gles2_;
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;

  // Used if !support_gles2_interface.
  std::unique_ptr<gpu::RasterInProcessContext> raster_context_;

  std::unique_ptr<ContextCacheController> cache_controller_;
  base::Optional<base::Lock> context_lock_;
  gpu::GpuFeatureInfo gpu_feature_info_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_
