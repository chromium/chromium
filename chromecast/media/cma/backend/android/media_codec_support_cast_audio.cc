// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "chromecast/public/media/media_capabilities_shlib.h"

namespace chromecast {
namespace media {

// TODO(ckuiper): This file is identical to
// alsa/media_codec_support_cast_audio.cc and should be consolidated.
bool MediaCapabilitiesShlib::IsSupportedVideoConfig(VideoCodec codec,
                                                    VideoProfile profile,
                                                    int level) {
  return (codec == kCodecH264 || codec == kCodecVP8 || codec == kCodecVP9);
}

bool MediaCapabilitiesShlib::IsSupportedAudioConfig(const AudioConfig& config) {
  return config.codec == kCodecAAC || config.codec == kCodecMP3 ||
         config.codec == kCodecPCM || config.codec == kCodecVorbis;
}

}  // namespace media
}  // namespace chromecast
