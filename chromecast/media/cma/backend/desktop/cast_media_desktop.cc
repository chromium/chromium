// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#if defined(ENABLE_VIDEO_WITH_MIXED_AUDIO)
#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h" // nogncheck
#else
#include "chromecast/media/cma/backend/desktop/media_pipeline_backend_desktop.h"
#endif
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/media/media_capabilities_shlib.h"
#include "chromecast/public/video_plane.h"

namespace chromecast {
namespace media {
namespace {

class DesktopVideoPlane : public VideoPlane {
 public:
  ~DesktopVideoPlane() override {}

  void SetGeometry(const RectF& display_rect, Transform transform) override {}
};

DesktopVideoPlane* g_video_plane = nullptr;

}  // namespace

void CastMediaShlib::Initialize(const std::vector<std::string>& argv) {
  g_video_plane = new DesktopVideoPlane();
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
#if defined(ENABLE_VIDEO_WITH_MIXED_AUDIO)
  return new MediaPipelineBackendForMixer(params);
#else
  return new MediaPipelineBackendDesktop();
#endif
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
  return (codec == kCodecH264 || codec == kCodecVP8);
}

bool MediaCapabilitiesShlib::IsSupportedAudioConfig(const AudioConfig& config) {
  return config.codec == kCodecAAC || config.codec == kCodecMP3 ||
         config.codec == kCodecPCM || config.codec == kCodecVorbis;
}

}  // namespace media
}  // namespace chromecast
