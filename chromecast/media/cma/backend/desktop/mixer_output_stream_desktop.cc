// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/desktop/mixer_output_stream_desktop.h"

#include <memory>

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "media/base/audio_timestamp_helper.h"

namespace chromecast {
namespace media {

constexpr base::TimeDelta kTargetWritePeriod = base::Milliseconds(20);

bool MixerOutputStreamDesktop::Start(int requested_sample_rate, int channels) {
  sample_rate_ = requested_sample_rate;
  channels_ = channels;
  target_packet_size_ = ::media::AudioTimestampHelper::TimeToFrames(
      kTargetWritePeriod, sample_rate_);
  return true;
}

int MixerOutputStreamDesktop::GetNumChannels() {
  return channels_;
}

int MixerOutputStreamDesktop::GetSampleRate() {
  return sample_rate_;
}

MediaPipelineBackend::AudioDecoder::RenderingDelay
MixerOutputStreamDesktop::GetRenderingDelay() {
  return MediaPipelineBackend::AudioDecoder::RenderingDelay();
}

int MixerOutputStreamDesktop::OptimalWriteFramesCount() {
  return target_packet_size_;
}

bool MixerOutputStreamDesktop::Write(const float* data,
                                     int data_size,
                                     bool* out_playback_interrupted) {
  int frames = data_size / channels_;
  auto frames_duration =
      ::media::AudioTimestampHelper::FramesToTime(frames, sample_rate_);
  base::PlatformThread::Sleep(frames_duration);

  return true;
}

void MixerOutputStreamDesktop::Stop() {}

// static
std::unique_ptr<MixerOutputStream> MixerOutputStream::Create() {
  return std::make_unique<MixerOutputStreamDesktop>();
}

}  // namespace media
}  // namespace chromecast
