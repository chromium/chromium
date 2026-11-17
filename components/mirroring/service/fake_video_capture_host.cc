// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/fake_video_capture_host.h"

#include <algorithm>

#include "base/memory/read_only_shared_memory_region.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/cast/test/utility/video_utility.h"

namespace mirroring {

namespace {

// Video buffer parameters.
constexpr bool kNotPremapped = false;

}  // namespace

FakeVideoCaptureHost::FakeVideoCaptureHost(
    mojo::PendingReceiver<media::mojom::VideoCaptureHost> receiver)
    : receiver_(this, std::move(receiver)) {}

FakeVideoCaptureHost::~FakeVideoCaptureHost() {
  Stop(base::UnguessableToken());
}

void FakeVideoCaptureHost::Start(
    const base::UnguessableToken& device_id,
    const base::UnguessableToken& session_id,
    const media::VideoCaptureParams& params,
    mojo::PendingRemote<media::mojom::VideoCaptureObserver> observer) {
  ASSERT_TRUE(observer);
  last_params_ = params;
  observer_.Bind(std::move(observer));
  observer_->OnStateChanged(media::mojom::VideoCaptureResult::NewState(
      media::mojom::VideoCaptureState::STARTED));
  if (on_started_cb_) {
    std::move(on_started_cb_).Run();
  }
}

void FakeVideoCaptureHost::Stop(const base::UnguessableToken& device_id) {
  if (!observer_) {
    return;
  }

  observer_->OnStateChanged(media::mojom::VideoCaptureResult::NewState(
      media::mojom::VideoCaptureState::ENDED));
  observer_.reset();
  OnStopped();
}

void FakeVideoCaptureHost::Pause(const base::UnguessableToken& device_id) {
  paused_ = true;
  OnPaused();
}

void FakeVideoCaptureHost::Resume(const base::UnguessableToken& device_id,
                                  const base::UnguessableToken& session_id,
                                  const media::VideoCaptureParams& params) {
  paused_ = false;
  OnResumed();
}

void FakeVideoCaptureHost::SendOneFrame(const gfx::Size& size,
                                        base::TimeTicks capture_time,
                                        media::VideoPixelFormat format,
                                        int start_value) {
  if (!observer_) {
    return;
  }

  // Memory management:
  // 1. Allocate a new ReadOnlySharedMemoryRegion for each test frame. For small
  //    test frames (e.g. 320x240 NV12 is ~115 KB), per-frame allocation is
  //    lightweight (< 1 MB across a test) and avoids buffer pool complexity.
  // 2. Populate pixel data via the writable `shmem.mapping`.
  // 3. Transfer ownership of `shmem.region` over Mojo via `OnNewBuffer` to the
  //    observer (VideoCaptureClient), which maps it into its buffer pool and
  //    manages its lifetime until the frame is processed and released.
  const size_t allocation_size =
      media::VideoFrame::AllocationSize(format, size);
  base::MappedReadOnlyRegion shmem =
      base::ReadOnlySharedMemoryRegion::Create(allocation_size);
  if (!shmem.IsValid()) {
    return;
  }

  const base::TimeDelta timestamp = capture_time - base::TimeTicks();
  scoped_refptr<media::VideoFrame> frame = media::VideoFrame::WrapExternalData(
      format, size, gfx::Rect(size), size,
      shmem.mapping.GetMemoryAsSpan<uint8_t>(), timestamp);
  if (frame) {
    media::cast::PopulateVideoFrame(frame.get(), start_value);
  } else {
    std::ranges::fill(shmem.mapping.GetMemoryAsSpan<uint8_t>(), 125);
  }

  const int32_t buffer_id = next_buffer_id_++;
  observer_->OnNewBuffer(
      buffer_id, media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
                     std::move(shmem.region)));
  media::VideoFrameMetadata metadata;
  metadata.frame_rate = 30;
  metadata.reference_time = capture_time;
  media::mojom::ReadyBufferPtr buffer = media::mojom::ReadyBuffer::New(
      buffer_id, media::mojom::VideoFrameInfo::New(
                     timestamp, metadata, format, size, gfx::Rect(size),
                     /*natural_size=*/size, kNotPremapped,
                     gfx::ColorSpace::CreateREC709(), nullptr));
  observer_->OnBufferReady(std::move(buffer));
}

media::VideoCaptureParams FakeVideoCaptureHost::GetVideoCaptureParams() const {
  return last_params_;
}

FakeVideoCaptureHost::PooledBuffer::PooledBuffer() = default;
FakeVideoCaptureHost::PooledBuffer::PooledBuffer(PooledBuffer&&) = default;
FakeVideoCaptureHost::PooledBuffer&
FakeVideoCaptureHost::PooledBuffer::operator=(PooledBuffer&&) = default;
FakeVideoCaptureHost::PooledBuffer::~PooledBuffer() = default;

int32_t FakeVideoCaptureHost::SendFrame(const media::VideoFrame& source,
                                        base::TimeTicks capture_time) {
  if (!observer_) {
    return -1;
  }
  CHECK_EQ(source.format(), media::PIXEL_FORMAT_I420);
  const gfx::Size size = source.visible_rect().size();
  const size_t allocation_size =
      media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420, size);

  // Find a free buffer of the right size, or allocate a new one.
  int32_t buffer_id = -1;
  for (auto& [id, buffer] : buffer_pool_) {
    if (!buffer.in_use && buffer.size == allocation_size) {
      buffer_id = id;
      break;
    }
  }
  if (buffer_id < 0) {
    base::MappedReadOnlyRegion shmem =
        base::ReadOnlySharedMemoryRegion::Create(allocation_size);
    if (!shmem.IsValid()) {
      return -1;
    }
    buffer_id = next_buffer_id_++;
    PooledBuffer buffer;
    buffer.mapping = std::move(shmem.mapping);
    buffer.size = allocation_size;
    buffer_pool_.emplace(buffer_id, std::move(buffer));
    observer_->OnNewBuffer(
        buffer_id, media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
                       std::move(shmem.region)));
  }

  PooledBuffer& buffer = buffer_pool_.at(buffer_id);
  buffer.in_use = true;

  const base::TimeDelta timestamp = capture_time - base::TimeTicks();
  scoped_refptr<media::VideoFrame> frame = media::VideoFrame::WrapExternalData(
      media::PIXEL_FORMAT_I420, size, gfx::Rect(size), size,
      buffer.mapping.GetMemoryAsSpan<uint8_t>(), timestamp);
  CHECK(frame);
  for (auto plane : {media::VideoFrame::Plane::kY, media::VideoFrame::Plane::kU,
                     media::VideoFrame::Plane::kV}) {
    const int rows =
        media::VideoFrame::Rows(plane, media::PIXEL_FORMAT_I420, size.height());
    const int row_bytes = media::VideoFrame::RowBytes(
        plane, media::PIXEL_FORMAT_I420, size.width());
    base::span<const uint8_t> src = source.GetVisiblePlaneData(plane);
    base::span<uint8_t> dst = frame->GetWritableVisiblePlaneData(plane);
    for (int row = 0; row < rows; ++row) {
      dst.subspan(static_cast<size_t>(row * frame->stride(plane)),
                  static_cast<size_t>(row_bytes))
          .copy_from(
              src.subspan(static_cast<size_t>(row * source.stride(plane)),
                          static_cast<size_t>(row_bytes)));
    }
  }

  media::VideoFrameMetadata metadata;
  metadata.frame_rate = 30;
  metadata.reference_time = capture_time;
  metadata.capture_begin_time = capture_time;
  metadata.capture_end_time = capture_time;
  media::mojom::ReadyBufferPtr ready = media::mojom::ReadyBuffer::New(
      buffer_id, media::mojom::VideoFrameInfo::New(
                     timestamp, metadata, media::PIXEL_FORMAT_I420, size,
                     gfx::Rect(size), /*natural_size=*/size, kNotPremapped,
                     gfx::ColorSpace::CreateREC709(), nullptr));
  observer_->OnBufferReady(std::move(ready));
  return buffer_id;
}

void FakeVideoCaptureHost::ReturnBuffer(int32_t buffer_id) {
  auto it = buffer_pool_.find(buffer_id);
  if (it != buffer_pool_.end()) {
    it->second.in_use = false;
  }
}

}  // namespace mirroring
