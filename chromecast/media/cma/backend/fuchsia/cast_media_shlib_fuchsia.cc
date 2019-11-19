// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/cast_media_shlib.h"

#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/video_plane.h"
#include "media/base/media.h"

namespace chromecast {
namespace media {

namespace {

// 1 MHz reference allows easy translation between frequency and PPM.
const double kMediaClockFrequency = 1e6;

class DefaultVideoPlane : public VideoPlane {
 public:
  ~DefaultVideoPlane() override {}

  void SetGeometry(const RectF& display_rect, Transform transform) override {}
};

DefaultVideoPlane* g_video_plane = nullptr;

}  // namespace

void CastMediaShlib::Initialize(const std::vector<std::string>& argv) {
  // On Fuchsia CastMediaShlib is compiled statically with cast_shell, so |argv|
  // can be ignored.

  g_video_plane = new DefaultVideoPlane();

  ::media::InitializeMediaLibrary();
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
  return new MediaPipelineBackendForMixer(params);
}

double CastMediaShlib::GetMediaClockRate() {
  return kMediaClockFrequency;
}

double CastMediaShlib::MediaClockRatePrecision() {
  return 1.0;
}

void CastMediaShlib::MediaClockRateRange(double* minimum_rate,
                                         double* maximum_rate) {
  *minimum_rate = kMediaClockFrequency;
  *maximum_rate = kMediaClockFrequency;
}

bool CastMediaShlib::SetMediaClockRate(double new_rate) {
  NOTIMPLEMENTED();
  return new_rate == kMediaClockFrequency;
}

bool CastMediaShlib::SupportsMediaClockRateChange() {
  return false;
}

}  // namespace media
}  // namespace chromecast
