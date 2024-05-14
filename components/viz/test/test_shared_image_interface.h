// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_SHARED_IMAGE_INTERFACE_H_
#define COMPONENTS_VIZ_TEST_TEST_SHARED_IMAGE_INTERFACE_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/ipc/client/shared_image_interface_proxy.h"

namespace viz {

class TestSharedImageInterface : public gpu::SharedImageInterface {
 public:
  TestSharedImageInterface();

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gpu::SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle) override;

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gpu::SharedImageInfo& si_info,
      base::span<const uint8_t> pixel_data) override;

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gpu::SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage) override;

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gpu::SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      gfx::GpuMemoryBufferHandle buffer_handle) override;

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gpu::SharedImageInfo& si_info,
      gfx::GpuMemoryBufferHandle buffer_handle) override;

  SharedImageInterface::SharedImageMapping CreateSharedImage(
      const gpu::SharedImageInfo& si_info) override;

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gfx::BufferPlane plane,
      const gpu::SharedImageInfo& si_info) override;

  void UpdateSharedImage(const gpu::SyncToken& sync_token,
                         const gpu::Mailbox& mailbox) override;
  void UpdateSharedImage(const gpu::SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const gpu::Mailbox& mailbox) override;

  scoped_refptr<gpu::ClientSharedImage> ImportSharedImage(
      const gpu::ExportedSharedImage& exported_shared_image) override;

  void DestroySharedImage(const gpu::SyncToken& sync_token,
                          const gpu::Mailbox& mailbox) override;
  void DestroySharedImage(
      const gpu::SyncToken& sync_token,
      scoped_refptr<gpu::ClientSharedImage> client_shared_image) override;

  SwapChainSharedImages CreateSwapChain(SharedImageFormat format,
                                        const gfx::Size& size,
                                        const gfx::ColorSpace& color_space,
                                        GrSurfaceOrigin surface_origin,
                                        SkAlphaType alpha_type,
                                        uint32_t usage) override;
  void PresentSwapChain(const gpu::SyncToken& sync_token,
                        const gpu::Mailbox& mailbox) override;

#if BUILDFLAG(IS_FUCHSIA)
  void RegisterSysmemBufferCollection(zx::eventpair service_handle,
                                      zx::channel sysmem_token,
                                      gfx::BufferFormat format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe) override;
#endif  // BUILDFLAG(IS_FUCHSIA)

  gpu::SyncToken GenVerifiedSyncToken() override;
  gpu::SyncToken GenUnverifiedSyncToken() override;
  void VerifySyncToken(gpu::SyncToken& sync_token) override;
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

  const gpu::SharedImageCapabilities& GetCapabilities() override;
  void SetCapabilities(const gpu::SharedImageCapabilities& caps);

  void SetFailSharedImageCreationWithBufferUsage(bool value) {
    fail_shared_image_creation_with_buffer_usage_ = value;
  }

  void UseTestGMBInSharedImageCreationWithBufferUsage() {
    test_gmb_manager_ = std::make_unique<gpu::TestGpuMemoryBufferManager>();
  }

 protected:
  ~TestSharedImageInterface() override;

 private:
  mutable base::Lock lock_;

  uint64_t release_id_ = 0;
  gfx::Size most_recent_size_;
  gpu::SyncToken most_recent_generated_token_;
  gpu::SyncToken most_recent_destroy_token_;
  base::flat_set<gpu::Mailbox> shared_images_;

  gpu::SharedImageCapabilities shared_image_capabilities_;
  bool fail_shared_image_creation_with_buffer_usage_ = false;

  // If non-null, this will be used to back mappable SharedImages with test
  // GpuMemoryBuffers.
  std::unique_ptr<gpu::TestGpuMemoryBufferManager> test_gmb_manager_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_SHARED_IMAGE_INTERFACE_H_
