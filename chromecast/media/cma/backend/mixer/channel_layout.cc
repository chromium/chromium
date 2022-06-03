// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/channel_layout.h"

#include "base/check_op.h"

namespace chromecast {
namespace media {
namespace mixer {

::media::ChannelLayout GuessChannelLayout(int num_channels) {
  if (num_channels > ::media::kMaxConcurrentChannels) {
    return ::media::CHANNEL_LAYOUT_DISCRETE;
  }
  return ::media::GuessChannelLayout(num_channels);
}

::media::AudioParameters CreateAudioParameters(
    ::media::AudioParameters::Format format,
    ::media::ChannelLayout channel_layout,
    int num_channels,
    int sample_rate,
    int frames_per_buffer) {
  ::media::AudioParameters parameters(format, channel_layout, sample_rate,
                                      frames_per_buffer);
  if (channel_layout == ::media::CHANNEL_LAYOUT_DISCRETE) {
    parameters.set_channels_for_discrete(num_channels);
  }
  return parameters;
}

::media::AudioParameters CreateAudioParametersForChannelMixer(
    ::media::ChannelLayout channel_layout,
    int num_channels) {
  if (channel_layout == ::media::CHANNEL_LAYOUT_NONE) {
    channel_layout = GuessChannelLayout(num_channels);
  }
  if (channel_layout != ::media::CHANNEL_LAYOUT_DISCRETE) {
    DCHECK_EQ(num_channels,
              ::media::ChannelLayoutToChannelCount(channel_layout));
  }
  // Sample rate and frames per buffer don't matter for channel mixer.
  return CreateAudioParameters(::media::AudioParameters::AUDIO_PCM_LINEAR,
                               channel_layout, num_channels, 48000, 1024);
}

}  // namespace mixer
}  // namespace media
}  // namespace chromecast
