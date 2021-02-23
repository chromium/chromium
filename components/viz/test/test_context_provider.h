// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_TEST_TEST_CONTEXT_PROVIDER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/service/display_embedder/viz_process_context_provider.h"
#include "components/viz/test/test_context_support.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/config/gpu_feature_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace viz {
class TestGLES2Interface;

class TestSharedImageInterface : public gpu::SharedImageInterface {
 public:
  TestSharedImageInterface();
  ~TestSharedImageInterface() override;

  gpu::Mailbox CreateSharedImage(ResourceFormat format,
                                 const gfx::Size& size,
                                 const gfx::ColorSpace& color_space,
                                 GrSurfaceOrigin surface_origin,
                                 SkAlphaType alpha_type,
                                 uint32_t usage,
                                 gpu::SurfaceHandle surface_handle) override;

  gpu::Mailbox CreateSharedImage(ResourceFormat format,
                                 const gfx::Size& size,
                                 const gfx::ColorSpace& color_space,
                                 GrSurfaceOrigin surface_origin,
                                 SkAlphaType alpha_type,
                                 uint32_t usage,
                                 base::span<const uint8_t> pixel_data) override;

  gpu::Mailbox CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) override;

  gpu::Mailbox CreateSharedImageWithAHB(
      const gpu::Mailbox& mailbox,
      uint32_t usage,
      const gpu::SyncToken& sync_token) override;

  void UpdateSharedImage(const gpu::SyncToken& sync_token,
                         const gpu::Mailbox& mailbox) override;
  void UpdateSharedImage(const gpu::SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const gpu::Mailbox& mailbox) override;

  void DestroySharedImage(const gpu::SyncToken& sync_token,
                          const gpu::Mailbox& mailbox) override;

  SwapChainMailboxes CreateSwapChain(ResourceFormat format,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space,
                                     GrSurfaceOrigin surface_origin,
                                     SkAlphaType alpha_type,
                                     uint32_t usage) override;
  void PresentSwapChain(const gpu::SyncToken& sync_token,
                        const gpu::Mailbox& mailbox) override;

#if defined(OS_FUCHSIA)
  void RegisterSysmemBufferCollection(gfx::SysmemBufferCollectionId id,
                                      zx::channel token,
                                      gfx::BufferFormat format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe) override;
  void ReleaseSysmemBufferCollection(gfx::SysmemBufferCollectionId id) override;
#endif  // defined(OS_FUCHSIA)

  gpu::SyncToken GenVerifiedSyncToken() override;
  gpu::SyncToken GenUnverifiedSyncToken() override;
  void WaitSyncToken(const gpu::SyncToken& sync_token) override;

  void Flush() override;
  scoped_refptr<gfx::NativePixmap> GetNativePixmap(
      const gpu::Mailbox& mailbox) override;

  size_t shared_image_count() const { return shared_images_.size(); }
  const gfx::Size& MostRecentSize() const { return most_recent_size_; }
  const gpu::SyncToken& MostRecentGeneratedToken() const {
    return most_recent_generated_token_;
  }
  const gpu::SyncToken& MostRecentDestroyToken() const {
    return most_recent_destroy_token_;
  }
  bool CheckSharedImageExists(const gpu::Mailbox& mailbox) const;

 private:
  mutable base::Lock lock_;

  uint64_t release_id_ = 0;
  gfx::Size most_recent_size_;
  gpu::SyncToken most_recent_generated_token_;
  gpu::SyncToken most_recent_destroy_token_;
  base::flat_set<gpu::Mailbox> shared_images_;
};

class TestContextProvider
    : public base::RefCountedThreadSafe<TestContextProvider>,
      public ContextProvider,
      public RasterContextProvider {
 public:
  static scoped_refptr<TestContextProvider> Create(
      std::string additional_extensions = std::string());
  // Creates a worker context provider that can be used on any thread. This is
  // equivalent to: Create(); BindToCurrentThread().
  static scoped_refptr<TestContextProvider> CreateWorker();
  static scoped_refptr<TestContextProvider> CreateWorker(
      std::unique_ptr<TestContextSupport> support);
  static scoped_refptr<TestContextProvider> Create(
      std::unique_ptr<TestGLES2Interface> gl);
  static scoped_refptr<TestContextProvider> Create(
      std::unique_ptr<TestSharedImageInterface> sii);
  static scoped_refptr<TestContextProvider> Create(
      std::unique_ptr<TestContextSupport> support);

  explicit TestContextProvider(std::unique_ptr<TestContextSupport> support,
                               std::unique_ptr<TestGLES2Interface> gl,
                               std::unique_ptr<TestSharedImageInterface> sii,
                               bool support_locking);
  explicit TestContextProvider(
      std::unique_ptr<TestContextSupport> support,
      std::unique_ptr<TestGLES2Interface> gl,
      std::unique_ptr<gpu::raster::RasterInterface> raster,
      std::unique_ptr<TestSharedImageInterface> sii,
      bool support_locking);

  // ContextProvider / RasterContextProvider implementation.
  void AddRef() const override;
  void Release() const override;
  gpu::ContextResult BindToCurrentThread() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::raster::RasterInterface* RasterInterface() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrDirectContext* GrContext() override;
  TestSharedImageInterface* SharedImageInterface() override;
  ContextCacheController* CacheController() override;
  base::Lock* GetLock() override;
  void AddObserver(ContextLostObserver* obs) override;
  void RemoveObserver(ContextLostObserver* obs) override;

  TestGLES2Interface* TestContextGL();
  // This returns the TestGLES2Interface but is valid to call
  // before the context is bound to a thread. This is needed to set up
  // state on the test interface before binding.
  TestGLES2Interface* UnboundTestContextGL() { return context_gl_.get(); }

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
  std::unique_ptr<TestGLES2Interface> context_gl_;
  std::unique_ptr<gpu::raster::RasterInterface> raster_context_;
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;
  std::unique_ptr<ContextCacheController> cache_controller_;
  std::unique_ptr<TestSharedImageInterface> shared_image_interface_;
  const bool support_locking_ ALLOW_UNUSED_TYPE;
  bool bound_ = false;

  gpu::GpuFeatureInfo gpu_feature_info_;

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  base::Lock context_lock_;

  base::ObserverList<ContextLostObserver>::Unchecked observers_;

  base::WeakPtrFactory<TestContextProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestContextProvider);
};

class TestVizProcessContextProvider : public VizProcessContextProvider {
 public:
  TestVizProcessContextProvider(std::unique_ptr<TestContextSupport> support,
                                std::unique_ptr<TestGLES2Interface> gl);
  TestVizProcessContextProvider(const TestVizProcessContextProvider&) = delete;
  TestVizProcessContextProvider& operator=(
      const TestVizProcessContextProvider&) = delete;

  // ContextProvider implementation.
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;

  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;
  void SetGpuVSyncCallback(GpuVSyncCallback callback) override;
  void SetGpuVSyncEnabled(bool enabled) override;
  bool UseRGB565PixelFormat() const override;
  uint32_t GetCopyTextureInternalFormat() override;
  base::ScopedClosureRunner GetCacheBackBufferCb() override;

 protected:
  ~TestVizProcessContextProvider() override;

 private:
  std::unique_ptr<TestContextSupport> support_;
  std::unique_ptr<TestGLES2Interface> context_gl_;
  gpu::Capabilities gpu_capabilities_;
  gpu::GpuFeatureInfo gpu_feature_info_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_CONTEXT_PROVIDER_H_
