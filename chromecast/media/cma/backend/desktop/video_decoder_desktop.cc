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
  CHECK(!sink_, base::NotFatalUntil::M163);
  sink_ = std::make_unique<MediaSinkDesktop>(delegate_, start_pts);
}

void VideoDecoderDesktop::Stop() {
  CHECK(sink_, base::NotFatalUntil::M163);
  sink_.reset();
}

void VideoDecoderDesktop::SetPlaybackRate(float rate) {
  CHECK(sink_, base::NotFatalUntil::M163);
  sink_->SetPlaybackRate(rate);
}

base::TimeDelta VideoDecoderDesktop::GetCurrentPts() {
  CHECK(sink_, base::NotFatalUntil::M163);
  return sink_->GetCurrentPts();
}

void VideoDecoderDesktop::SetDelegate(Delegate* delegate) {
  CHECK(!sink_, base::NotFatalUntil::M163);
  delegate_ = delegate;
}

MediaPipelineBackend::BufferStatus VideoDecoderDesktop::PushBuffer(
    CastDecoderBuffer* buffer) {
  CHECK(sink_, base::NotFatalUntil::M163);
  return sink_->PushBuffer(buffer);
}

void VideoDecoderDesktop::GetStatistics(Statistics* statistics) {}

bool VideoDecoderDesktop::SetConfig(const VideoConfig& config) {
  return true;
}

}  // namespace media
}  // namespace chromecast
