// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_MIME_UTILS_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_MIME_UTILS_H_

#include <cstdint>
#include <string>

#include "chromecast/public/media/decoder_config.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"

namespace chromecast {
namespace media {

// Returns the MIME string for the given video codec/profile/level. Container is
// guessed in a way that should be compatible with Starboard's checks (e.g. for
// HEVC we guess MP4). Ideally Starboard should not care about the container,
// since they do not handle demuxing.
//
// If a MIME type cannot be determined, an empty string is returned.
std::string GetMimeType(VideoCodec codec, VideoProfile profile, int32_t level);

// Same as above, but uses chromium enums.
std::string GetMimeType(::media::VideoCodec codec,
                        ::media::VideoCodecProfile profile,
                        uint32_t level);

// Returns the MIME string for the given audio codec. Container is guessed in a
// way that should be compatible with Starboard's checks (e.g. for opus we guess
// webm). Ideally Starboard should not care about the container, since they do
// not handle demuxing.
//
// If a MIME type cannot be determined, an empty string is returned.
std::string GetMimeType(AudioCodec codec);

// Same as above, but uses the chromium version of the codec enum.
std::string GetMimeType(::media::AudioCodec codec);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_MIME_UTILS_H_
