// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_capture/gpu_memory_buffer_video_frame_pool.h"

#include <utility>

#include "components/viz/service/frame_sinks/gmb_video_frame_pool_context_provider.h"

namespace viz {

GpuMemoryBufferVideoFramePool::GpuMemoryBufferVideoFramePool(
    int capacity,
    media::VideoPixelFormat format,
    const gfx::ColorSpace& color_space,
    GmbVideoFramePoolContextProvider* context_provider)
    : VideoFramePool(capacity),
      format_(format),
      color_space_(color_space),
      context_provider_(context_provider) {
  RecreateVideoFramePool();
}

GpuMemoryBufferVideoFramePool::~GpuMemoryBufferVideoFramePool() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<media::VideoFrame>
GpuMemoryBufferVideoFramePool::ReserveVideoFrame(media::VideoPixelFormat format,
                                                 const gfx::Size& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(format, format_) << "Reserving a format that is different from the "
                               "one specified in the constructor.";
  CHECK_LE(num_reserved_frames_, capacity());

  if (num_reserved_frames_ == capacity()) {
    return nullptr;
  }

  scoped_refptr<media::VideoFrame> result =
      video_frame_pool_->MaybeCreateVideoFrame(size, color_space_);

  if (result) {
    num_reserved_frames_++;
    result->AddDestructionObserver(base::BindOnce(
        &GpuMemoryBufferVideoFramePool::OnVideoFrameDestroyed,
        weak_factory_.GetWeakPtr(), video_frame_pool_generation_));
  }

  return result;
}

media::mojom::VideoBufferHandlePtr
GpuMemoryBufferVideoFramePool::CloneHandleForDelivery(
    const media::VideoFrame& frame) {
  CHECK(frame.HasMappableGpuBuffer());

  gfx::GpuMemoryBufferHandle handle = frame.GetGpuMemoryBufferHandle();
  handle.id = gfx::GpuMemoryBufferHandle::kInvalidId;

  return media::mojom::VideoBufferHandle::NewGpuMemoryBufferHandle(
      std::move(handle));
}

size_t GpuMemoryBufferVideoFramePool::GetNumberOfReservedFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return num_reserved_frames_;
}

void GpuMemoryBufferVideoFramePool::RecreateVideoFramePool() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto pool_context = context_provider_->CreateContext(
      base::BindOnce(&GpuMemoryBufferVideoFramePool::RecreateVideoFramePool,
                     weak_factory_.GetWeakPtr()));
  video_frame_pool_ = media::RenderableGpuMemoryBufferVideoFramePool::Create(
      std::move(pool_context), format_);

  video_frame_pool_generation_++;
  num_reserved_frames_ = 0;
}

void GpuMemoryBufferVideoFramePool::OnVideoFrameDestroyed(
    uint32_t frame_pool_generation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (frame_pool_generation == video_frame_pool_generation_) {
    CHECK_GT(num_reserved_frames_, 0u);

    num_reserved_frames_--;
  }
}

}  // namespace viz
