// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/logging.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/media/cma/backend/android/media_pipeline_backend_android.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/video_plane.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {
namespace {

class DefaultVideoPlane : public VideoPlane {
 public:
  ~DefaultVideoPlane() override {}

  void SetGeometry(const RectF& display_rect, Transform transform) override {}
};

const char* GetContentTypeName(const AudioContentType type) {
  switch (type) {
    case AudioContentType::kMedia:
      return "kMedia";
    case AudioContentType::kAlarm:
      return "kAlarm";
    case AudioContentType::kCommunication:
      return "kCommunication";
    default:
      return "Unknown";
  }
}

DefaultVideoPlane* g_video_plane = nullptr;

}  // namespace

void CastMediaShlib::Initialize(const std::vector<std::string>& argv) {
  LOG(INFO) << __func__ << ":";

  g_video_plane = new DefaultVideoPlane();
}

void CastMediaShlib::Finalize() {
  LOG(INFO) << __func__;

  delete g_video_plane;
  g_video_plane = nullptr;
}

VideoPlane* CastMediaShlib::GetVideoPlane() {
  return g_video_plane;
}

MediaPipelineBackend* CastMediaShlib::CreateMediaPipelineBackend(
    const MediaPipelineDeviceParams& params) {
  LOG(INFO) << __func__ << ":"
            << " sync_type=" << params.sync_type
            << " audio_type=" << params.audio_type
            << " content_type=" << GetContentTypeName(params.content_type)
            << " device_id=" << params.device_id;
  return new MediaPipelineBackendAndroid(params);
}

double CastMediaShlib::GetMediaClockRate() {
  NOTREACHED();
}

double CastMediaShlib::MediaClockRatePrecision() {
  NOTREACHED();
}

void CastMediaShlib::MediaClockRateRange(double* minimum_rate,
                                         double* maximum_rate) {
  NOTREACHED();
}

bool CastMediaShlib::SetMediaClockRate(double new_rate) {
  NOTREACHED();
}

bool CastMediaShlib::SupportsMediaClockRateChange() {
  LOG(INFO) << __func__ << ":";
  return false;
}

}  // namespace media
}  // namespace chromecast
