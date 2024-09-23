// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_TEST_TEST_CONTEXT_PROVIDER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/test/test_context_support.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/config/gpu_feature_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace viz {
class TestGLES2Interface;
class TestRasterInterface;

class TestContextProvider
    : public base::RefCountedThreadSafe<TestContextProvider>,
      public ContextProvider,
      public RasterContextProvider {
 public:
  // Creates a context backed by TestGLES2Interface with no lock.
  static scoped_refptr<TestContextProvider> Create(
      std::string additional_extensions = std::string());
  static scoped_refptr<TestContextProvider> Create(
      std::unique_ptr<TestGLES2Interface> gl);
  static scoped_refptr<TestContextProvider> Create(
      scoped_refptr<gpu::TestSharedImageInterface> sii);

  // Creates a context backed by TestRasterInterface with no lock.
  static scoped_refptr<TestContextProvider> CreateRaster();
  static scoped_refptr<TestContextProvider> CreateRaster(
      std::unique_ptr<TestRasterInterface> raster);
  static scoped_refptr<TestContextProvider> CreateRaster(
      std::unique_ptr<TestContextSupport> support);

  // Creates a worker context provider that can be used on any thread. This is
  // equivalent to: Create(); BindToCurrentSequence().
  static scoped_refptr<TestContextProvider> CreateWorker();
  static scoped_refptr<TestContextProvider> CreateWorker(
      std::unique_ptr<TestContextSupport> support);

  explicit TestContextProvider(std::unique_ptr<TestContextSupport> support,
                               std::unique_ptr<TestRasterInterface> raster,
                               bool support_locking);
  explicit TestContextProvider(
      std::unique_ptr<TestContextSupport> support,
      std::unique_ptr<TestGLES2Interface> gl,
      std::unique_ptr<gpu::raster::RasterInterface> raster,
      scoped_refptr<gpu::TestSharedImageInterface> sii,
      bool support_locking);

  TestContextProvider(const TestContextProvider&) = delete;
  TestContextProvider& operator=(const TestContextProvider&) = delete;

  // ContextProvider / RasterContextProvider implementation.
  void AddRef() const override;
  void Release() const override;
  gpu::ContextResult BindToCurrentSequence() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::raster::RasterInterface* RasterInterface() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrDirectContext* GrContext() override;
  gpu::TestSharedImageInterface* SharedImageInterface() override;
  ContextCacheController* CacheController() override;
  base::Lock* GetLock() override;
  void AddObserver(ContextLostObserver* obs) override;
  void RemoveObserver(ContextLostObserver* obs) override;
  unsigned int GetGrGLTextureFormat(SharedImageFormat format) const override;

  TestGLES2Interface* TestContextGL();
  TestRasterInterface* GetTestRasterInterface();

  // This returns the TestGLES2Interface but is valid to call
  // before the context is bound to a thread. This is needed to set up
  // state on the test interface before binding.
  TestGLES2Interface* UnboundTestContextGL() { return context_gl_.get(); }
  TestRasterInterface* UnboundTestRasterInterface();

  TestContextSupport* support() { return support_.get(); }

  gpu::GpuFeatureInfo& GetWritableGpuFeatureInfo() { return gpu_feature_info_; }

 protected:
  friend class base::RefCountedThreadSafe<TestContextProvider>;
  ~TestContextProvider() override;

 private:
  void OnLostContext();
  void CheckValidThreadOrLockAcquired() const {
#if DCHECK_IS_ON()
    if (support_locking_) {
      context_lock_.AssertAcquired();
    } else {
      DCHECK(context_thread_checker_.CalledOnValidThread());
    }
#endif
  }

  std::unique_ptr<TestContextSupport> support_;

  // Used for GLES2 contexts.
  std::unique_ptr<TestGLES2Interface> context_gl_;
  std::unique_ptr<gpu::raster::RasterInterface> raster_interface_gles_;
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;

  // Used for raster contexts.
  std::unique_ptr<TestRasterInterface> raster_context_;

  std::unique_ptr<ContextCacheController> cache_controller_;
  scoped_refptr<gpu::TestSharedImageInterface> shared_image_interface_;
  [[maybe_unused]] const bool support_locking_;
  bool bound_ = false;

  gpu::GpuFeatureInfo gpu_feature_info_;

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  base::Lock context_lock_;

  base::ObserverList<ContextLostObserver>::Unchecked observers_;

  base::WeakPtrFactory<TestContextProvider> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_CONTEXT_PROVIDER_H_
