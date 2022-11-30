// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_SUPPORTED_CODEC_PROFILE_LEVELS_MEMO_H_
#define CHROMECAST_MEDIA_BASE_SUPPORTED_CODEC_PROFILE_LEVELS_MEMO_H_

#include <vector>

#include "base/synchronization/lock.h"
#include "chromecast/public/media/decoder_config.h"

namespace chromecast {
namespace media {

// Keeps a memo in the render process for a list of supported codecs and
// profiles obtained from the browser process.
// All calls should be made in the main renderer thread.
class SupportedCodecProfileLevelsMemo {
 public:
  SupportedCodecProfileLevelsMemo();

  SupportedCodecProfileLevelsMemo(const SupportedCodecProfileLevelsMemo&) =
      delete;
  SupportedCodecProfileLevelsMemo& operator=(
      const SupportedCodecProfileLevelsMemo&) = delete;

  ~SupportedCodecProfileLevelsMemo();
  void AddSupportedCodecProfileLevel(CodecProfileLevel codec_profile_level);
  bool IsSupportedVideoConfig(VideoCodec codec,
                              VideoProfile profile,
                              int level) const;

 private:
  mutable base::Lock lock_;
  std::vector<CodecProfileLevel> codec_profile_levels_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_SUPPORTED_CODEC_PROFILE_LEVELS_MEMO_H_
