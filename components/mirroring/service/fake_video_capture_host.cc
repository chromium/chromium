// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/fake_video_capture_host.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"

namespace mirroring {

FakeVideoCaptureHost::FakeVideoCaptureHost(
    mojo::PendingReceiver<media::mojom::VideoCaptureHost> receiver)
    : receiver_(this, std::move(receiver)) {}
FakeVideoCaptureHost::~FakeVideoCaptureHost() {}

void FakeVideoCaptureHost::Start(
    const base::UnguessableToken& device_id,
    const base::UnguessableToken& session_id,
    const media::VideoCaptureParams& params,
    mojo::PendingRemote<media::mojom::VideoCaptureObserver> observer) {
  ASSERT_TRUE(observer);
  observer_.Bind(std::move(observer));
  observer_->OnStateChanged(media::mojom::VideoCaptureState::STARTED);
}

void FakeVideoCaptureHost::Stop(const base::UnguessableToken& device_id) {
  if (!observer_)
    return;

  observer_->OnStateChanged(media::mojom::VideoCaptureState::ENDED);
  observer_.reset();
  OnStopped();
}

void FakeVideoCaptureHost::SendOneFrame(const gfx::Size& size,
                                        base::TimeTicks capture_time) {
  if (!observer_)
    return;

  auto shmem = mojo::CreateReadOnlySharedMemoryRegion(5000);
  memset(shmem.mapping.memory(), 125, 5000);
  observer_->OnNewBuffer(
      0, media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
             std::move(shmem.region)));
  media::VideoFrameMetadata metadata;
  metadata.SetDouble(media::VideoFrameMetadata::FRAME_RATE, 30);
  metadata.SetTimeTicks(media::VideoFrameMetadata::REFERENCE_TIME,
                        capture_time);
  observer_->OnBufferReady(
      0, media::mojom::VideoFrameInfo::New(
             base::TimeDelta(), metadata.GetInternalValues().Clone(),
             media::PIXEL_FORMAT_I420, size, gfx::Rect(size),
             gfx::ColorSpace::CreateREC709(), nullptr));
}

}  // namespace mirroring
