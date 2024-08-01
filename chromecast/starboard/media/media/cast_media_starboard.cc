// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/media_capabilities_shlib.h"
#include "chromecast/public/video_plane.h"
#include "chromecast/starboard/media/media/media_pipeline_backend_starboard.h"
#include "chromecast/starboard/media/media/mime_utils.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/media/media/starboard_video_plane.h"

namespace chromecast {
namespace media {
namespace {

StarboardVideoPlane* g_video_plane = nullptr;

// Returns true if starboard supports Dolby Vision playback for HEVC content
// with the given profile and level.
bool IsHEVCDolbyVisionSupported(VideoProfile profile, int level) {
  // A leak is intentional here, since starboard_api has static storage
  // duration.
  static StarboardApiWrapper* const starboard_api =
      GetStarboardApiWrapper().release();

  const std::string mime = GetMimeType(kCodecDolbyVisionHEVC, profile, level);
  if (mime.empty()) {
    return false;
  }

  const bool supported =
      starboard_api->CanPlayMimeAndKeySystem(mime.c_str(), /*key_system=*/"") ==
      kStarboardMediaSupportTypeProbably;
  LOG(INFO) << "HEVC profile=" << profile << ", level=" << level << " is "
            << (supported ? "supported" : "not supported");
  return supported;
}

}  // namespace

void CastMediaShlib::Initialize(const std::vector<std::string>& argv) {
  CHECK(g_video_plane == nullptr);
  g_video_plane = new StarboardVideoPlane();
}

void CastMediaShlib::Finalize() {
  delete g_video_plane;
  g_video_plane = nullptr;
}

VideoPlane* CastMediaShlib::GetVideoPlane() {
  return g_video_plane;
}

MediaPipelineBackend* CastMediaShlib::CreateMediaPipelineBackend(
    const MediaPipelineDeviceParams& params) {
  CHECK(g_video_plane);
  return new MediaPipelineBackendStarboard(params, g_video_plane);
}

double CastMediaShlib::GetMediaClockRate() {
  return 0.0;
}

double CastMediaShlib::MediaClockRatePrecision() {
  return 0.0;
}

void CastMediaShlib::MediaClockRateRange(double* minimum_rate,
                                         double* maximum_rate) {
  *minimum_rate = 0.0;
  *maximum_rate = 1.0;
}

bool CastMediaShlib::SetMediaClockRate(double new_rate) {
  return false;
}

bool CastMediaShlib::SupportsMediaClockRateChange() {
  return false;
}

VideoPlane::Coordinates VideoPlane::GetCoordinates() {
  // SbPlayerSetBounds takes coordinates in terms of the graphics resolution.
  return VideoPlane::Coordinates::kGraphics;
}

bool MediaCapabilitiesShlib::IsSupportedVideoConfig(VideoCodec codec,
                                                    VideoProfile profile,
                                                    int level) {
  // TODO(b/275430044): expand this to cover all video codecs/profiles/levels.
  if (codec == kCodecDolbyVisionHEVC) {
    return IsHEVCDolbyVisionSupported(profile, level);
  }

  return codec == kCodecH264 || codec == kCodecVP8 || codec == kCodecVP9 ||
         codec == kCodecHEVC;
}

bool MediaCapabilitiesShlib::IsSupportedAudioConfig(const AudioConfig& config) {
  // TODO(b/275430044): potentially call Starboard here, to determine codec
  // support. This is probably only necessary for optional codecs (AC3/E-AC3).
  return config.codec == kCodecAAC || config.codec == kCodecMP3 ||
         config.codec == kCodecPCM || config.codec == kCodecVorbis ||
         config.codec == kCodecOpus || config.codec == kCodecAC3 ||
         config.codec == kCodecEAC3 || config.codec == kCodecFLAC;
}

}  // namespace media
}  // namespace chromecast
