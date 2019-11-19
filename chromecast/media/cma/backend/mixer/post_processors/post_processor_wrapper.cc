// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/post_processors/post_processor_wrapper.h"

#include "base/logging.h"
#include "chromecast/public/media/audio_post_processor_shlib.h"

namespace chromecast {
namespace media {

AudioPostProcessorWrapper::AudioPostProcessorWrapper(
    std::unique_ptr<AudioPostProcessor> pp,
    int channels)
    : owned_pp_(std::move(pp)), pp_(owned_pp_.get()) {
  DCHECK(owned_pp_);
  status_.output_channels = channels;
}

AudioPostProcessorWrapper::AudioPostProcessorWrapper(AudioPostProcessor* pp,
                                                     int channels)
    : pp_(pp) {
  DCHECK(pp_);
  status_.output_channels = channels;
}

AudioPostProcessorWrapper::~AudioPostProcessorWrapper() = default;

bool AudioPostProcessorWrapper::SetConfig(
    const AudioPostProcessor2::Config& config) {
  if (!pp_->SetSampleRate(config.output_sample_rate)) {
    return false;
  }
  status_.input_sample_rate = config.output_sample_rate;
  status_.ringing_time_frames = pp_->GetRingingTimeInFrames();
  return true;
}

const AudioPostProcessor2::Status& AudioPostProcessorWrapper::GetStatus() {
  return status_;
}

void AudioPostProcessorWrapper::ProcessFrames(float* data,
                                              int frames,
                                              float system_volume,
                                              float volume_dbfs) {
  status_.output_buffer = data;
  status_.rendering_delay_frames =
      pp_->ProcessFrames(data, frames, system_volume, volume_dbfs);
}

bool AudioPostProcessorWrapper::UpdateParameters(const std::string& message) {
  pp_->UpdateParameters(message);
  return true;
}

void AudioPostProcessorWrapper::SetContentType(AudioContentType content_type) {
  pp_->SetContentType(content_type);
}

void AudioPostProcessorWrapper::SetPlayoutChannel(int channel) {
  pp_->SetPlayoutChannel(channel);
}

}  // namespace media
}  // namespace chromecast
