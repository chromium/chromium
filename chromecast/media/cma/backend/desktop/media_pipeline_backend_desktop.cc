// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/desktop/media_pipeline_backend_desktop.h"

#include "base/check_op.h"
#include "base/time/time.h"
#include "chromecast/media/cma/backend/desktop/audio_decoder_desktop.h"
#include "chromecast/media/cma/backend/desktop/video_decoder_desktop.h"
#include "media/base/timestamp_constants.h"

namespace chromecast {
namespace media {

MediaPipelineBackendDesktop::MediaPipelineBackendDesktop()
    : state_(kStateUninitialized), rate_(1.0f) {}

MediaPipelineBackendDesktop::~MediaPipelineBackendDesktop() {}

MediaPipelineBackend::AudioDecoder*
MediaPipelineBackendDesktop::CreateAudioDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  DCHECK(!audio_decoder_);
  audio_decoder_ = std::make_unique<AudioDecoderDesktop>();
  return audio_decoder_.get();
}

MediaPipelineBackend::VideoDecoder*
MediaPipelineBackendDesktop::CreateVideoDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  DCHECK(!video_decoder_);
  video_decoder_ = std::make_unique<VideoDecoderDesktop>();
  return video_decoder_.get();
}

bool MediaPipelineBackendDesktop::Initialize() {
  DCHECK_EQ(kStateUninitialized, state_);
  state_ = kStateInitialized;
  return true;
}

bool MediaPipelineBackendDesktop::Start(int64_t start_pts) {
  DCHECK_EQ(kStateInitialized, state_);
  if (!audio_decoder_ && !video_decoder_)
    return false;

  if (audio_decoder_) {
    audio_decoder_->Start(base::Microseconds(start_pts));
    audio_decoder_->SetPlaybackRate(rate_);
  }
  if (video_decoder_) {
    video_decoder_->Start(base::Microseconds(start_pts));
    video_decoder_->SetPlaybackRate(rate_);
  }
  state_ = kStatePlaying;
  return true;
}

void MediaPipelineBackendDesktop::Stop() {
  DCHECK(state_ == kStatePlaying || state_ == kStatePaused);
  if (audio_decoder_)
    audio_decoder_->Stop();
  if (video_decoder_)
    video_decoder_->Stop();
  state_ = kStateInitialized;
}

bool MediaPipelineBackendDesktop::Pause() {
  DCHECK_EQ(kStatePlaying, state_);
  if (audio_decoder_)
    audio_decoder_->SetPlaybackRate(0.0f);
  if (video_decoder_)
    video_decoder_->SetPlaybackRate(0.0f);
  state_ = kStatePaused;
  return true;
}

bool MediaPipelineBackendDesktop::Resume() {
  DCHECK_EQ(kStatePaused, state_);
  if (audio_decoder_)
    audio_decoder_->SetPlaybackRate(rate_);
  if (video_decoder_)
    video_decoder_->SetPlaybackRate(rate_);
  state_ = kStatePlaying;
  return true;
}

int64_t MediaPipelineBackendDesktop::GetCurrentPts() {
  base::TimeDelta current_pts = ::media::kNoTimestamp;

  if (audio_decoder_ && video_decoder_) {
    current_pts = std::min(audio_decoder_->GetCurrentPts(),
                           video_decoder_->GetCurrentPts());
  } else if (audio_decoder_) {
    current_pts = audio_decoder_->GetCurrentPts();
  } else if (video_decoder_) {
    current_pts = video_decoder_->GetCurrentPts();
  }

  return current_pts.InMicroseconds();
}

bool MediaPipelineBackendDesktop::SetPlaybackRate(float rate) {
  DCHECK_GT(rate, 0.0f);
  rate_ = rate;

  if (state_ == kStatePlaying) {
    if (audio_decoder_)
      audio_decoder_->SetPlaybackRate(rate_);
    if (video_decoder_)
      video_decoder_->SetPlaybackRate(rate_);
  }

  return true;
}

}  // namespace media
}  // namespace chromecast
