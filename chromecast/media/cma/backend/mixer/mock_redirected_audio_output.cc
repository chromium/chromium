// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mock_redirected_audio_output.h"

#include <algorithm>

#include "base/check.h"
#include "media/base/audio_bus.h"

using testing::_;

namespace chromecast {
namespace media {

MockRedirectedAudioOutput::MockRedirectedAudioOutput(
    const mixer_service::RedirectedAudioConnection::Config& config)
    : config_(config),
      connection_(config_, this),
      last_output_timestamp_(INT64_MIN) {
  ON_CALL(*this, OnRedirectedAudio(_, _, _))
      .WillByDefault(testing::Invoke(
          this, &MockRedirectedAudioOutput::HandleRedirectedAudio));
  connection_.Connect();
}

MockRedirectedAudioOutput::~MockRedirectedAudioOutput() = default;

void MockRedirectedAudioOutput::SetStreamMatchPatterns(
    std::vector<std::pair<AudioContentType, std::string>> patterns) {
  connection_.SetStreamMatchPatterns(std::move(patterns));
}

void MockRedirectedAudioOutput::HandleRedirectedAudio(int64_t timestamp,
                                                      float* data,
                                                      int frames) {
  CHECK(data);
  last_buffer_ = ::media::AudioBus::Create(config_.num_output_channels, frames);
  for (int c = 0; c < config_.num_output_channels; ++c) {
    std::copy_n(data + c * frames, frames, last_buffer_->channel(c));
  }

  last_output_timestamp_ = timestamp;
}

}  // namespace media
}  // namespace chromecast
