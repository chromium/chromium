// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions for converting chromium structs to equivalent starboard
// structs.

#ifndef CHROMECAST_STARBOARD_MEDIA_RENDERER_CHROMIUM_STARBOARD_CONVERSIONS_H_
#define CHROMECAST_STARBOARD_MEDIA_RENDERER_CHROMIUM_STARBOARD_CONVERSIONS_H_

#include <optional>

#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"

namespace chromecast {
namespace media {

// Converts an AudioDecoderConfig to StarboardAudioSampleInfo, returning nullopt
// if the config is not supported or is invalid.
std::optional<StarboardAudioSampleInfo> ToStarboardAudioSampleInfo(
    const ::media::AudioDecoderConfig& in_config);

// Converts a VideoDecoderConfig to StarboardVideoSampleInfo, returning nullopt
// if the config is not supported or is invalid.
std::optional<StarboardVideoSampleInfo> ToStarboardVideoSampleInfo(
    const ::media::VideoDecoderConfig& in_config);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_RENDERER_CHROMIUM_STARBOARD_CONVERSIONS_H_
