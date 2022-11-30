// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/desktop/video_decoder_desktop.h"

#include "chromecast/media/cma/backend/desktop/media_sink_desktop.h"

namespace chromecast {
namespace media {

VideoDecoderDesktop::VideoDecoderDesktop() {}

VideoDecoderDesktop::~VideoDecoderDesktop() {}

void VideoDecoderDesktop::Start(base::TimeDelta start_pts) {
  DCHECK(!sink_);
  sink_ = std::make_unique<MediaSinkDesktop>(delegate_, start_pts);
}

void VideoDecoderDesktop::Stop() {
  DCHECK(sink_);
  sink_.reset();
}

void VideoDecoderDesktop::SetPlaybackRate(float rate) {
  DCHECK(sink_);
  sink_->SetPlaybackRate(rate);
}

base::TimeDelta VideoDecoderDesktop::GetCurrentPts() {
  DCHECK(sink_);
  return sink_->GetCurrentPts();
}

void VideoDecoderDesktop::SetDelegate(Delegate* delegate) {
  DCHECK(!sink_);
  delegate_ = delegate;
}

MediaPipelineBackend::BufferStatus VideoDecoderDesktop::PushBuffer(
    CastDecoderBuffer* buffer) {
  DCHECK(sink_);
  return sink_->PushBuffer(buffer);
}

void VideoDecoderDesktop::GetStatistics(Statistics* statistics) {}

bool VideoDecoderDesktop::SetConfig(const VideoConfig& config) {
  return true;
}

}  // namespace media
}  // namespace chromecast
