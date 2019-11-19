// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/android/media_pipeline_backend_android.h"

#include <limits>

#include "base/logging.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/backend/android/audio_decoder_android.h"
#include "chromecast/media/cma/backend/video_decoder_null.h"

namespace chromecast {
namespace media {

MediaPipelineBackendAndroid::MediaPipelineBackendAndroid(
    const MediaPipelineDeviceParams& params)
    : state_(kStateUninitialized), params_(params) {
  LOG(INFO) << __func__ << ":";
}

MediaPipelineBackendAndroid::~MediaPipelineBackendAndroid() {}

MediaPipelineBackendAndroid::AudioDecoder*
MediaPipelineBackendAndroid::CreateAudioDecoder() {
  LOG(INFO) << __func__ << ":";
  DCHECK_EQ(kStateUninitialized, state_);
  if (audio_decoder_)
    return nullptr;
  audio_decoder_.reset(new AudioDecoderAndroid(this));
  return audio_decoder_.get();
}

MediaPipelineBackendAndroid::VideoDecoder*
MediaPipelineBackendAndroid::CreateVideoDecoder() {
  LOG(INFO) << __func__ << ":";
  DCHECK_EQ(kStateUninitialized, state_);
  if (video_decoder_)
    return nullptr;
  video_decoder_.reset(new VideoDecoderNull());
  return video_decoder_.get();
}

bool MediaPipelineBackendAndroid::Initialize() {
  LOG(INFO) << __func__ << ":";
  DCHECK_EQ(kStateUninitialized, state_);
  if (audio_decoder_)
    audio_decoder_->Initialize();
  state_ = kStateInitialized;
  return true;
}

bool MediaPipelineBackendAndroid::Start(int64_t start_pts) {
  LOG(INFO) << __func__ << ": start_pts=" << start_pts;
  DCHECK_EQ(kStateInitialized, state_);
  if (audio_decoder_ && !audio_decoder_->Start(start_pts))
    return false;
  state_ = kStatePlaying;
  return true;
}

void MediaPipelineBackendAndroid::Stop() {
  LOG(INFO) << __func__ << ":";
  DCHECK(state_ == kStatePlaying || state_ == kStatePaused)
      << "Invalid state " << state_;
  if (audio_decoder_)
    audio_decoder_->Stop();

  state_ = kStateInitialized;
}

bool MediaPipelineBackendAndroid::Pause() {
  LOG(INFO) << __func__ << ":";
  DCHECK_EQ(kStatePlaying, state_);
  if (audio_decoder_ && !audio_decoder_->Pause())
    return false;
  state_ = kStatePaused;
  return true;
}

bool MediaPipelineBackendAndroid::Resume() {
  LOG(INFO) << __func__ << ":";
  DCHECK_EQ(kStatePaused, state_);
  if (audio_decoder_ && !audio_decoder_->Resume())
    return false;
  state_ = kStatePlaying;
  return true;
}

bool MediaPipelineBackendAndroid::SetPlaybackRate(float rate) {
  LOG(INFO) << __func__ << ": rate=" << rate;
  if (audio_decoder_) {
    return audio_decoder_->SetPlaybackRate(rate);
  }
  return true;
}

int64_t MediaPipelineBackendAndroid::GetCurrentPts() {
  if (audio_decoder_) {
    int64_t pts = audio_decoder_->current_pts();
    DVLOG(2) << __func__ << ": pts=" << pts;
    return pts;
  }
  LOG(INFO) << __func__ << ": pts=<invalid>";
  return std::numeric_limits<int64_t>::min();
}

bool MediaPipelineBackendAndroid::Primary() const {
  return (params_.audio_type !=
          MediaPipelineDeviceParams::kAudioStreamSoundEffects);
}

std::string MediaPipelineBackendAndroid::DeviceId() const {
  return params_.device_id;
}

AudioContentType MediaPipelineBackendAndroid::ContentType() const {
  return params_.content_type;
}

AudioChannel MediaPipelineBackendAndroid::AudioChannel() const {
  return params_.audio_channel;
}

const scoped_refptr<base::SingleThreadTaskRunner>&
MediaPipelineBackendAndroid::GetTaskRunner() const {
  return static_cast<TaskRunnerImpl*>(params_.task_runner)->runner();
}

}  // namespace media
}  // namespace chromecast
