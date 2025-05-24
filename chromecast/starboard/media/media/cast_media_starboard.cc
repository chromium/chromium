// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chromecast/base/cast_features.h"
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

// Returns a handle to starboard.
//
// TODO(crbug.com/396728662): this should return a const reference.
StarboardApiWrapper& GetStarboardHandle() {
  // A leak is intentional here, since starboard_api has static storage
  // duration.
  static StarboardApiWrapper* const starboard_api = []() {
    std::unique_ptr<StarboardApiWrapper> starboard = GetStarboardApiWrapper();
    CHECK(starboard);
    return starboard.release();
  }();

  return *starboard_api;
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
  static bool enable_mime_checks =
      base::FeatureList::IsEnabled(kEnableStarboardMimeChecks);

  // We always need to check dolby vision, since not all starboard linux TVs
  // support the codec at all. Other codecs were expected to be supported (at
  // least for some profiles/levels).
  if (!enable_mime_checks && codec != kCodecDolbyVisionHEVC) {
    return codec == kCodecH264 || codec == kCodecVP8 || codec == kCodecVP9 ||
           codec == kCodecHEVC;
  }

  const std::string mime = GetMimeType(codec, profile, level);
  if (mime.empty()) {
    return false;
  }

  const bool supported = GetStarboardHandle().CanPlayMimeAndKeySystem(
                             mime.c_str(), /*key_system=*/"") ==
                         kStarboardMediaSupportTypeProbably;

  LOG(INFO) << "Video codec=" << codec << ", profile=" << profile
            << ", level=" << level << " is "
            << (supported ? "supported" : "not supported") << " by starboard.";
  return supported;
}

bool MediaCapabilitiesShlib::IsSupportedAudioConfig(const AudioConfig& config) {
  static bool enable_mime_checks =
      base::FeatureList::IsEnabled(kEnableStarboardMimeChecks);

  if (!enable_mime_checks) {
    return config.codec == kCodecAAC || config.codec == kCodecMP3 ||
           config.codec == kCodecPCM || config.codec == kCodecVorbis ||
           config.codec == kCodecOpus || config.codec == kCodecAC3 ||
           config.codec == kCodecEAC3 || config.codec == kCodecFLAC;
  }

  const std::string mime = GetMimeType(config.codec);
  if (mime.empty()) {
    return false;
  }

  const bool supported = GetStarboardHandle().CanPlayMimeAndKeySystem(
                             mime.c_str(), /*key_system=*/"") ==
                         kStarboardMediaSupportTypeProbably;

  LOG(INFO) << "Audio codec=" << config.codec << " is "
            << (supported ? "supported" : "not supported") << " by starboard.";
  return supported;
}

}  // namespace media
}  // namespace chromecast
