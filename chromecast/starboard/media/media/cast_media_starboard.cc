// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/media_capabilities_shlib.h"
#include "chromecast/public/video_plane.h"
#include "media_pipeline_backend_starboard.h"
#include "starboard_video_plane.h"

namespace chromecast {
namespace media {
namespace {

StarboardVideoPlane* g_video_plane = nullptr;

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
  return new MediaPipelineBackendStarboard(g_video_plane);
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
  // TODO(b/275430044): potentially call Starboard here, to determine codec
  // support.
  return codec == kCodecH264 || codec == kCodecVP8 || codec == kCodecVP9 ||
         codec == kCodecHEVC;
}

bool MediaCapabilitiesShlib::IsSupportedAudioConfig(const AudioConfig& config) {
  // TODO(b/275430044): potentially call Starboard here, to determine codec
  // support.
  return config.codec == kCodecAAC || config.codec == kCodecMP3 ||
         config.codec == kCodecPCM || config.codec == kCodecVorbis ||
         config.codec == kCodecOpus || config.codec == kCodecAC3 ||
         config.codec == kCodecEAC3 || config.codec == kCodecFLAC;
}

}  // namespace media
}  // namespace chromecast
