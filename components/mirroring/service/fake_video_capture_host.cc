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

}  // namespace mirroring
