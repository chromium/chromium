// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_MIME_UTILS_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_MIME_UTILS_H_

#include <cstdint>
#include <string>

#include "chromecast/public/media/decoder_config.h"

namespace chromecast {
namespace media {

// TODO(b/275430044): Add support for all codecs here. For now, we only need
// this for Dolby Vision.
//
// Returns the MIME string for the given codec/profile/level. Container is
// guessed in a way that should be compatible with Starboard's checks (e.g. for
// HEVC we guess MP4). Ideally Starboard should not care about the container,
// since they do not handle demuxing.
//
// If a MIME type cannot be determined, an empty string is returned.
std::string GetMimeType(VideoCodec codec, VideoProfile profile, uint32_t level);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_MIME_UTILS_H_
