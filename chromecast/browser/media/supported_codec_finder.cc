// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/supported_codec_finder.h"

#include <vector>

#include "base/logging.h"
#include "build/build_config.h"
#include "chromecast/browser/media/media_caps_impl.h"
#include "chromecast/media/base/media_codec_support.h"
#if BUILDFLAG(IS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif
#include "media/base/video_codecs.h"

namespace chromecast {
namespace media {

void SupportedCodecFinder::FindSupportedCodecProfileLevels(
    MediaCapsImpl* media_caps) {
// Don't need to list supported codecs on non-Android devices.
#if BUILDFLAG(IS_ANDROID)
  // Get list of supported codecs from MediaCodec.
  std::vector<::media::CodecProfileLevel> codec_profile_levels;
  ::media::MediaCodecUtil::AddSupportedCodecProfileLevels(
      &codec_profile_levels);
  LOG(INFO) << "Adding " << codec_profile_levels.size()
            << " supported codec profiles/levels";
  for (const auto& codec_profile_level : codec_profile_levels) {
    media_caps->AddSupportedCodecProfileLevel(
        ToCastCodecProfileLevel(codec_profile_level));
  }
#endif
}

}  // namespace media
}  // namespace chromecast
