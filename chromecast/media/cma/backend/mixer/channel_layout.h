// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_CHANNEL_LAYOUT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_CHANNEL_LAYOUT_H_

#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"

namespace chromecast {
namespace media {
namespace mixer {

// Guesses the channel layout based on the number of channels; supports using
// CHANNEL_LAYOUT_DISCRETE is there are more than 8 channels.
::media::ChannelLayout GuessChannelLayout(int num_channels);

// Creates an AudioParameters with correct support for CHANNEL_LAYOUT_DISCRETE.
::media::AudioParameters CreateAudioParameters(
    ::media::AudioParameters::Format format,
    ::media::ChannelLayout channel_layout,
    int num_channels,
    int sample_rate,
    int frames_per_buffer);

// Creates AudioParameters for constructing a ChannelMixer.
::media::AudioParameters CreateAudioParametersForChannelMixer(
    ::media::ChannelLayout channel_layout,
    int num_channels);

}  // namespace mixer
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_CHANNEL_LAYOUT_H_
