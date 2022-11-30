// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_CONTEXT_SUPPORT_H_
#define COMPONENTS_VIZ_TEST_TEST_CONTEXT_SUPPORT_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/client/context_support.h"

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace viz {

class TestContextSupport : public gpu::ContextSupport {
 public:
  TestContextSupport();

  TestContextSupport(const TestContextSupport&) = delete;
  TestContextSupport& operator=(const TestContextSupport&) = delete;

  ~TestContextSupport() override;

  // gpu::ContextSupport implementation.
  void FlushPendingWork() override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       base::OnceClosure callback) override;
  bool IsSyncTokenSignaled(const gpu::SyncToken& sync_token) override;
  void SignalQuery(uint32_t query, base::OnceClosure callback) override;
  void GetGpuFence(uint32_t gpu_fence_id,
                   base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                       callback) override;
  void SetAggressivelyFreeResources(bool aggressively_free_resources) override;
  uint64_t ShareGroupTracingGUID() const override;
  void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t)> callback) override;
  bool ThreadSafeShallowLockDiscardableTexture(uint32_t texture_id) override;
  void CompleteLockDiscardableTexureOnContextThread(
      uint32_t texture_id) override;
  bool ThreadsafeDiscardableTextureIsDeletedForTracing(
      uint32_t texture_id) override;
  void* MapTransferCacheEntry(uint32_t serialized_size) override;
  void UnmapAndCreateTransferCacheEntry(uint32_t type, uint32_t id) override;
  bool ThreadsafeLockTransferCacheEntry(uint32_t entry_type,
                                        uint32_t entry_id) override;
  void UnlockTransferCacheEntries(
      const std::vector<std::pair<uint32_t, uint32_t>>& entries) override;
  void DeleteTransferCacheEntry(uint32_t entry_type,
                                uint32_t entry_id) override;
  unsigned int GetTransferBufferFreeSize() const override;
  bool IsJpegDecodeAccelerationSupported() const override;
  bool IsWebPDecodeAccelerationSupported() const override;
  bool CanDecodeWithHardwareAcceleration(
      const cc::ImageHeaderMetadata* image_metadata) const override;
  bool HasGrContextSupport() const override;
  void SetGrContext(GrDirectContext* gr) override;
  void WillCallGLFromSkia() override;
  void DidCallGLFromSkia() override;

  void CallAllSyncPointCallbacks();

  // If set true, callbacks triggering will be in a reverse order as SignalQuery
  // calls.
  void set_out_of_order_callbacks(bool out_of_order_callbacks) {
    out_of_order_callbacks_ = out_of_order_callbacks;
  }

 private:
  std::vector<base::OnceClosure> sync_point_callbacks_;
  bool out_of_order_callbacks_;

  base::WeakPtrFactory<TestContextSupport> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_CONTEXT_SUPPORT_H_
