// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/logging.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/media_capabilities_shlib.h"
#include "chromecast/public/video_plane.h"
#include "chromecast/starboard/chromecast/starboard_adapter/public/cast_starboard_api_adapter.h"
#include "chromecast/starboard/media/media/media_pipeline_backend_starboard.h"
#include "chromecast/starboard/media/media/mime_utils.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/media/media/starboard_video_plane.h"

namespace chromecast {
namespace media {
namespace {

StarboardVideoPlane* g_video_plane = nullptr;

// Returns true if the MIME type `mime` is probably supported by starboard
// (false otherwise).
bool IsSupportedByStarboard(const std::string& mime) {
  // A leak is intentional here, since starboard has static storage duration.
  static StarboardApiWrapper* const starboard = []() {
    std::unique_ptr<StarboardApiWrapper> inner_starboard =
        GetStarboardApiWrapper();
    CHECK(inner_starboard);
    return inner_starboard.release();
  }();

  // We subscribe to CastStarboardApiAdapter to ensure that starboard has been
  // initialized.
  int context = 0;
  CastStarboardApiAdapter::GetInstance()->Subscribe(&context, nullptr);
  const bool supported =
      starboard->CanPlayMimeAndKeySystem(mime.c_str(), /*key_system=*/"") ==
      kStarboardMediaSupportTypeProbably;
  CastStarboardApiAdapter::GetInstance()->Unsubscribe(&context);

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

bool MediaCapabilitiesShlib::IsSupportedVideoConfig(VideoCodec codec,
                                                    VideoProfile profile,
                                                    int level) {
  static const bool enable_mime_checks =
      base::FeatureList::IsEnabled(kEnableStarboardMimeChecks);

  static const bool enable_av1_checks =
      base::FeatureList::IsEnabled(kEnableStarboardAv1Checks);

  if (codec == kCodecAV1 && !enable_av1_checks) {
    LOG(INFO)
        << "AV1 support is disabled (add enable_starboard_av1_checks to the "
           "enable-features runtime flag in order to check for AV1 support).";
    return false;
  }

  // We always need to check dolby vision and AV1, since not all starboard linux
  // TVs support those codecs at all. Other codecs were expected to be supported
  // (at least for some profiles/levels).
  if (!enable_mime_checks && codec != kCodecDolbyVisionHEVC &&
      codec != kCodecAV1) {
    return codec == kCodecH264 || codec == kCodecVP8 || codec == kCodecVP9 ||
           codec == kCodecHEVC;
  }

  const std::string mime = GetMimeType(codec, profile, level);
  if (mime.empty()) {
    return false;
  }

  const bool supported = IsSupportedByStarboard(mime);
  LOG(INFO) << "Video codec=" << codec << ", profile=" << profile
            << ", level=" << level << " (MIME type " << mime << ") is "
            << (supported ? "supported" : "not supported") << " by starboard.";
  return supported;
}

bool MediaCapabilitiesShlib::IsSupportedAudioConfig(const AudioConfig& config) {
  static const bool enable_mime_checks =
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

  const bool supported = IsSupportedByStarboard(mime);
  LOG(INFO) << "Audio codec=" << config.codec << " (MIME type " << mime
            << ") is " << (supported ? "supported" : "not supported")
            << " by starboard.";
  return supported;
}

}  // namespace media
}  // namespace chromecast
