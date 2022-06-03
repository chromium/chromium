// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video_decoder_null.h"

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/public/media/cast_decoder_buffer.h"

namespace chromecast {
namespace media {

std::unique_ptr<VideoDecoderForMixer> VideoDecoderForMixer::Create(
    const MediaPipelineDeviceParams& params) {
  return std::make_unique<VideoDecoderNull>();
}

void VideoDecoderForMixer::InitializeGraphicsForTesting() {
  // No initialization required
}

VideoDecoderNull::VideoDecoderNull()
    : delegate_(nullptr), weak_factory_(this) {}

VideoDecoderNull::~VideoDecoderNull() {}

void VideoDecoderNull::SetDelegate(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

MediaPipelineBackend::BufferStatus VideoDecoderNull::PushBuffer(
    CastDecoderBuffer* buffer) {
  DCHECK(delegate_);
  DCHECK(buffer);
  if (buffer->end_of_stream()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&VideoDecoderNull::OnEndOfStream,
                                  weak_factory_.GetWeakPtr()));
  }
  return MediaPipelineBackend::kBufferSuccess;
}

void VideoDecoderNull::GetStatistics(Statistics* statistics) {}

bool VideoDecoderNull::SetConfig(const VideoConfig& config) {
  return true;
}

void VideoDecoderNull::OnEndOfStream() {
  delegate_->OnEndOfStream();
}

bool VideoDecoderNull::Initialize() {
  return true;
}

void VideoDecoderNull::SetObserver(VideoDecoderForMixer::Observer* observer) {
  DCHECK(observer);
  observer_ = observer;
}

bool VideoDecoderNull::Start(int64_t start_pts, bool need_avsync) {
  if (observer_) {
    observer_->VideoReadyToPlay();
  }
  return true;
}

void VideoDecoderNull::Stop() {}

bool VideoDecoderNull::Pause() {
  return true;
}

bool VideoDecoderNull::Resume() {
  return true;
}

bool VideoDecoderNull::GetCurrentPts(int64_t* timestamp, int64_t* pts) const {
  return false;
}

bool VideoDecoderNull::SetPlaybackRate(float rate) {
  return true;
}

bool VideoDecoderNull::SetPts(int64_t timestamp, int64_t pts) {
  return true;
}

int64_t VideoDecoderNull::GetDroppedFrames() {
  return 0;
}

int64_t VideoDecoderNull::GetRepeatedFrames() {
  return 0;
}

int64_t VideoDecoderNull::GetOutputRefreshRate() {
  return 0;
}

int64_t VideoDecoderNull::GetCurrentContentRefreshRate() {
  return 0;
}

}  // namespace media
}  // namespace chromecast
