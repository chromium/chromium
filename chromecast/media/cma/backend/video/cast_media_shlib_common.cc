// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/media/media_capabilities_shlib.h"

namespace chromecast {
namespace media {

bool MediaCapabilitiesShlib::IsSupportedAudioConfig(const AudioConfig& config) {
  switch (config.codec) {
    case kCodecPCM:
    case kCodecPCM_S16BE:
    case kCodecAAC:
    case kCodecMP3:
    case kCodecVorbis:
      return true;
    default:
      break;
  }
  return false;
}

}  // namespace media
}  // namespace chromecast
