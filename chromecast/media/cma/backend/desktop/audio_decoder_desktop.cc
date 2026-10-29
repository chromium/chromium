// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/desktop/audio_decoder_desktop.h"

#include "chromecast/media/cma/backend/desktop/media_sink_desktop.h"

namespace chromecast {
namespace media {

AudioDecoderDesktop::AudioDecoderDesktop() : delegate_(nullptr) {}

AudioDecoderDesktop::~AudioDecoderDesktop() {}

void AudioDecoderDesktop::Start(base::TimeDelta start_pts) {
  CHECK(!sink_, base::NotFatalUntil::M163);
  sink_ = std::make_unique<MediaSinkDesktop>(delegate_, start_pts);
}

void AudioDecoderDesktop::Stop() {
  CHECK(sink_, base::NotFatalUntil::M163);
  sink_.reset();
}

void AudioDecoderDesktop::SetPlaybackRate(float rate) {
  CHECK(sink_, base::NotFatalUntil::M163);
  sink_->SetPlaybackRate(rate);
}

base::TimeDelta AudioDecoderDesktop::GetCurrentPts() {
  CHECK(sink_, base::NotFatalUntil::M163);
  return sink_->GetCurrentPts();
}

void AudioDecoderDesktop::SetDelegate(Delegate* delegate) {
  CHECK(!sink_, base::NotFatalUntil::M163);
  delegate_ = delegate;
}

MediaPipelineBackend::BufferStatus AudioDecoderDesktop::PushBuffer(
    CastDecoderBuffer* buffer) {
  CHECK(sink_, base::NotFatalUntil::M163);
  return sink_->PushBuffer(buffer);
}

void AudioDecoderDesktop::GetStatistics(Statistics* statistics) {}

bool AudioDecoderDesktop::SetConfig(const AudioConfig& config) {
  return true;
}

bool AudioDecoderDesktop::SetVolume(float multiplier) {
  return true;
}

AudioDecoderDesktop::RenderingDelay AudioDecoderDesktop::GetRenderingDelay() {
  return RenderingDelay();
}

AudioDecoderDesktop::AudioTrackTimestamp AudioDecoderDesktop::GetAudioTrackTimestamp() {
  return AudioTrackTimestamp();
}

int AudioDecoderDesktop::GetStartThresholdInFrames() {
  return 0;
}

}  // namespace media
}  // namespace chromecast
