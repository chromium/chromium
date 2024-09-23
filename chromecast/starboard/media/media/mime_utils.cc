// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/mime_utils.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace chromecast {
namespace media {

namespace {

// Returns a MIME string for HEVC dolby vision.
//
// Returns an empty string if the MIME type is not supported by cast, or if it
// cannot be determined.
std::string GetHEVCDolbyVisionMimeType(VideoProfile profile, uint32_t level) {
  if (level >= 10 || level == 0) {
    // Note: there ARE valid dolby vision levels > 10, but they are not
    // officially supported by cast. See
    // https://developers.google.com/cast/docs/media and
    // https://professionalsupport.dolby.com/s/article/What-is-Dolby-Vision-Profile?language=en_US
    LOG(INFO) << "Unsupported dolby vision level: " << level;
    return "";
  }

  int profile_int = 0;

  // According to
  // https://professional.dolby.com/siteassets/content-creation/dolby-vision-for-content-creators/dolbyvisioninmpegdashspecification_v2_0_public_20190107.pdf,
  // only profiles 5 and 8 are supported for online streaming.
  switch (profile) {
    case kDolbyVisionProfile5:
      profile_int = 5;
      break;
    case kDolbyVisionProfile8:
      profile_int = 8;
      break;
    default:
      LOG(INFO) << "Unsupported dolby vision profile: " << profile;
      return "";
  }

  // 'level' is exactly one digit, due to the above check.
  return base::StringPrintf(R"-(video/mp4; codecs="dvhe.0%d.0%d")-",
                            profile_int, level);
}

}  // namespace

std::string GetMimeType(VideoCodec codec,
                        VideoProfile profile,
                        uint32_t level) {
  if (codec == kCodecDolbyVisionHEVC) {
    return GetHEVCDolbyVisionMimeType(profile, level);
  }
  return "";
}

}  // namespace media
}  // namespace chromecast
