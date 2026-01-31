// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_capture/mappable_shared_image_video_frame_pool.h"

#include <utility>

#include "components/viz/service/frame_sinks/gmb_video_frame_pool_context_provider.h"

namespace viz {

MappableSharedImageVideoFramePool::MappableSharedImageVideoFramePool(
    int capacity,
    media::VideoPixelFormat format,
    const gfx::ColorSpace& color_space,
    GmbVideoFramePoolContextProvider* context_provider,
    mojom::BufferFormatPreference buffer_format_preference)
    : VideoFramePool(capacity),
      format_(format),
      color_space_(color_space),
      context_provider_(context_provider),
      buffer_format_preference_(buffer_format_preference) {
  RecreateVideoFramePool();
}

MappableSharedImageVideoFramePool::~MappableSharedImageVideoFramePool() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<media::VideoFrame>
MappableSharedImageVideoFramePool::ReserveVideoFrame(
    media::VideoPixelFormat format,
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
        &MappableSharedImageVideoFramePool::OnVideoFrameDestroyed,
        weak_factory_.GetWeakPtr(), video_frame_pool_generation_));
  }

  return result;
}

media::mojom::VideoBufferHandlePtr
MappableSharedImageVideoFramePool::CloneHandleForDelivery(
    const media::VideoFrame& frame) {
  CHECK(frame.HasMappableSharedImage());

  gfx::GpuMemoryBufferHandle handle = frame.GetGpuMemoryBufferHandle();

  return media::mojom::VideoBufferHandle::NewGpuMemoryBufferHandle(
      std::move(handle));
}

size_t MappableSharedImageVideoFramePool::GetNumberOfReservedFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return num_reserved_frames_;
}

void MappableSharedImageVideoFramePool::RecreateVideoFramePool() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto pool_context = context_provider_->CreateContext(
      base::BindOnce(&MappableSharedImageVideoFramePool::RecreateVideoFramePool,
                     weak_factory_.GetWeakPtr()));
  // Determine whether the video frame pool should use CPU mappable buffers.
  // If the caller prefers SharedImage with native handle (i.e., no CPU
  // mapping), pass false. Otherwise, default to requiring CPU access.
  video_frame_pool_ =
      media::RenderableMappableSharedImageVideoFramePool::Create(
          std::move(pool_context), format_,
          buffer_format_preference_ != mojom::BufferFormatPreference::
                                           kPreferSharedImageWithNativeHandle);

  video_frame_pool_generation_++;
  num_reserved_frames_ = 0;
}

void MappableSharedImageVideoFramePool::OnVideoFrameDestroyed(
    uint32_t frame_pool_generation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (frame_pool_generation == video_frame_pool_generation_) {
    CHECK_GT(num_reserved_frames_, 0u);

    num_reserved_frames_--;
  }
}

}  // namespace viz
