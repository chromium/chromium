// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_MEDIA_CAPABILITIES_SHLIB_H_
#define CHROMECAST_PUBLIC_MEDIA_MEDIA_CAPABILITIES_SHLIB_H_

#include "chromecast_export.h"
#include "decoder_config.h"

namespace chromecast {
namespace media {

// Interface for specifying platform media capabilities. It allows for more
// detailed information to be provided by the platform compared to the previous
// MediaCodecSupportShlib interface.
class CHROMECAST_EXPORT MediaCapabilitiesShlib {
 public:
  // Return true if the current platform supports the given combination of video
  // codec, profile and level. For a list of supported codecs and profiles see
  // decoder_config.h. The level value is codec specific. For H.264 and VP9 the
  // level value is multiplied by ten, i.e. level=51 corresponds to level 5.1
  // For HEVC the level value is multiplied by 30, to match level_idc value in
  // HEVC bitstream. So for HEVC level=153 corresponds to level 5.1
  static bool IsSupportedVideoConfig(VideoCodec codec,
                                     VideoProfile profile,
                                     int level);

  // Return true if the platform supports the given audio |config|.
  static bool IsSupportedAudioConfig(const AudioConfig& config);

  // Return true if the platform is able to decode and display the video stream
  // smoothly with requested codec, profile, visible size and framerate.
  CHROMECAST_EXPORT static bool CanPlayVideoSmoothly(VideoCodec codec,
                                                     VideoProfile profile,
                                                     int width,
                                                     int height,
                                                     double framerate)
      __attribute__((weak));
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_MEDIA_CAPABILITIES_SHLIB_H_
