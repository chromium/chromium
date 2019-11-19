// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_CONTEXT_SUPPORT_H_
#define COMPONENTS_VIZ_TEST_TEST_CONTEXT_SUPPORT_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/client/context_support.h"

namespace gfx {
class GpuFence;
class Rect;
class RectF;
}  // namespace gfx

namespace viz {

class TestContextSupport : public gpu::ContextSupport {
 public:
  TestContextSupport();
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
  void Swap(uint32_t flags,
            SwapCompletedCallback complete_callback,
            PresentationCallback presentation_callback) override {}
  void SwapWithBounds(const std::vector<gfx::Rect>& rects,
                      uint32_t flags,
                      SwapCompletedCallback swap_completed,
                      PresentationCallback presentation_callback) override {}
  void PartialSwapBuffers(const gfx::Rect& sub_buffer,
                          uint32_t flags,
                          SwapCompletedCallback swap_completed,
                          PresentationCallback presentation_callback) override {
  }
  void CommitOverlayPlanes(
      uint32_t flags,
      SwapCompletedCallback swap_completed,
      PresentationCallback presentation_callback) override {}
  void ScheduleOverlayPlane(int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            unsigned overlay_texture_id,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& uv_rect,
                            bool enable_blend,
                            unsigned gpu_fence_id) override;
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
  void SetGrContext(GrContext* gr) override;
  void WillCallGLFromSkia() override;
  void DidCallGLFromSkia() override;
  void SetDisplayTransform(gfx::OverlayTransform transform) override {}

  void CallAllSyncPointCallbacks();

  using ScheduleOverlayPlaneCallback =
      base::RepeatingCallback<void(int plane_z_order,
                                   gfx::OverlayTransform plane_transform,
                                   unsigned overlay_texture_id,
                                   const gfx::Rect& display_bounds,
                                   const gfx::RectF& crop_rect,
                                   bool enable_blend,
                                   unsigned gpu_fence_id)>;
  void SetScheduleOverlayPlaneCallback(
      const ScheduleOverlayPlaneCallback& schedule_overlay_plane_callback);

  // If set true, callbacks triggering will be in a reverse order as SignalQuery
  // calls.
  void set_out_of_order_callbacks(bool out_of_order_callbacks) {
    out_of_order_callbacks_ = out_of_order_callbacks;
  }

 private:
  std::vector<base::OnceClosure> sync_point_callbacks_;
  ScheduleOverlayPlaneCallback schedule_overlay_plane_callback_;
  bool out_of_order_callbacks_;

  base::WeakPtrFactory<TestContextSupport> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestContextSupport);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_CONTEXT_SUPPORT_H_
