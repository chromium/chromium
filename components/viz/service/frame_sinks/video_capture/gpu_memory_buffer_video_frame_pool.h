// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_

#include <memory>

#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/frame_sinks/video_capture/video_frame_pool.h"
#include "components/viz/service/viz_service_export.h"
#include "media/video/renderable_gpu_memory_buffer_video_frame_pool.h"

namespace viz {

class GmbVideoFramePoolContextProvider;

class VIZ_SERVICE_EXPORT GpuMemoryBufferVideoFramePool : public VideoFramePool {
 public:
  // Creates new pool instance with specified |capacity|.
  // The |context_provider| must outlive this instance.
  explicit GpuMemoryBufferVideoFramePool(
      int capacity,
      media::VideoPixelFormat format,
      const gfx::ColorSpace& color_space,
      GmbVideoFramePoolContextProvider* context_provider);
  ~GpuMemoryBufferVideoFramePool() override;

  GpuMemoryBufferVideoFramePool(const GpuMemoryBufferVideoFramePool&& other) =
      delete;
  GpuMemoryBufferVideoFramePool& operator=(
      const GpuMemoryBufferVideoFramePool& other) = delete;

  // VideoFramePool implementation:
  scoped_refptr<media::VideoFrame> ReserveVideoFrame(
      media::VideoPixelFormat format,
      const gfx::Size& size) override;

  media::mojom::VideoBufferHandlePtr CloneHandleForDelivery(
      const media::VideoFrame& frame) override;

  size_t GetNumberOfReservedFrames() const override;

 private:
  void RecreateVideoFramePool();

  // Notify the pool that a video frame from |frame_pool_generation| was
  // destroyed. If the generation matches the current pool generation, the
  // number of reserved frames will be decremented.
  void OnVideoFrameDestroyed(uint32_t frame_pool_generation);

  // Support multiple format & color space
  const media::VideoPixelFormat format_;
  const gfx::ColorSpace color_space_;

  raw_ptr<GmbVideoFramePoolContextProvider> context_provider_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<media::RenderableGpuMemoryBufferVideoFramePool>
      video_frame_pool_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Number of reserved video frames.
  size_t num_reserved_frames_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // Current generation of |video_frame_pool_|. It will be bumped every time
  // we detect a context lost event and recerate |video_frame_pool_| - this is
  // needed so that we can correctly book-keep the number of reserved frames
  // (when a video frame from previous generation is released, we don't want
  // to decrement |num_reserved_frames_|).
  uint32_t video_frame_pool_generation_ GUARDED_BY_CONTEXT(sequence_checker_) =
      0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GpuMemoryBufferVideoFramePool> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
