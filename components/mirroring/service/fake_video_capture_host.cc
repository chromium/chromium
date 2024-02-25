// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/fake_video_capture_host.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"

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
}

void FakeVideoCaptureHost::Stop(const base::UnguessableToken& device_id) {
  if (!observer_)
    return;

  observer_->OnStateChanged(media::mojom::VideoCaptureResult::NewState(
      media::mojom::VideoCaptureState::ENDED));
  observer_.reset();
  OnStopped();
}

void FakeVideoCaptureHost::Pause(const base::UnguessableToken& device_id) {
  paused_ = true;
}

void FakeVideoCaptureHost::Resume(const base::UnguessableToken& device_id,
                                  const base::UnguessableToken& session_id,
                                  const media::VideoCaptureParams& params) {
  paused_ = false;
}

void FakeVideoCaptureHost::SendOneFrame(const gfx::Size& size,
                                        base::TimeTicks capture_time) {
  if (!observer_)
    return;

  auto shmem = base::ReadOnlySharedMemoryRegion::Create(5000);
  if (!shmem.IsValid()) {
    return;
  }
  memset(shmem.mapping.memory(), 125, 5000);
  observer_->OnNewBuffer(
      0, media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
             std::move(shmem.region)));
  media::VideoFrameMetadata metadata;
  metadata.frame_rate = 30;
  metadata.reference_time = capture_time;
  media::mojom::ReadyBufferPtr buffer = media::mojom::ReadyBuffer::New(
      0, media::mojom::VideoFrameInfo::New(
             base::TimeDelta(), metadata, media::PIXEL_FORMAT_I420, size,
             gfx::Rect(size), kNotPremapped, gfx::ColorSpace::CreateREC709(),
             nullptr));
  observer_->OnBufferReady(std::move(buffer));
}

media::VideoCaptureParams FakeVideoCaptureHost::GetVideoCaptureParams() const {
  return last_params_;
}

}  // namespace mirroring
