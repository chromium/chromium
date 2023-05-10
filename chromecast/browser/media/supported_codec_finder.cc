// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/supported_codec_finder.h"

#include <vector>

#include "build/build_config.h"
#include "chromecast/browser/media/media_caps_impl.h"
#include "chromecast/media/base/media_codec_support.h"
#if BUILDFLAG(IS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif
#include "media/base/video_codecs.h"

namespace chromecast::media {

// static
std::vector<CodecProfileLevel>
SupportedCodecFinder::FindSupportedCodecProfileLevels() {
  std::vector<CodecProfileLevel> cast_codec_profile_levels;
// Don't need to list supported codecs on non-Android devices.
#if BUILDFLAG(IS_ANDROID)
  // Get list of supported codecs from MediaCodec.
  std::vector<::media::CodecProfileLevel> codec_profile_levels;
  ::media::MediaCodecUtil::AddSupportedCodecProfileLevels(
      &codec_profile_levels);
  cast_codec_profile_levels.reserve(codec_profile_levels.size());
  for (const auto& codec_profile_level : codec_profile_levels) {
    cast_codec_profile_levels.push_back(
        ToCastCodecProfileLevel(codec_profile_level));
  }
#endif
  return cast_codec_profile_levels;
}

}  // namespace chromecast::media
