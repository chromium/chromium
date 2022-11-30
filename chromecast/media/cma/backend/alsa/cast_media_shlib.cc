// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <alsa/asoundlib.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/video_plane.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"

#define RETURN_ON_ALSA_ERROR(snd_func, ...)                    \
  do {                                                         \
    int err = snd_func(__VA_ARGS__);                           \
    if (err < 0) {                                             \
      LOG(ERROR) << #snd_func " error: " << snd_strerror(err); \
      return;                                                  \
    }                                                          \
  } while (0)

namespace chromecast {
namespace media {
namespace {

const char kDefaultPcmDevice[] = "hw:0";
const int kSoundControlBlockingMode = 0;
const char kRateOffsetInterfaceName[] = "PCM Playback Rate Offset";

// 1 MHz reference allows easy translation between frequency and PPM.
const double kOneMhzReference = 1e6;
const double kMaxAdjustmentHz = 500;
const double kGranularityHz = 1.0;

class DefaultVideoPlane : public VideoPlane {
 public:
  ~DefaultVideoPlane() override {}

  void SetGeometry(const RectF& display_rect, Transform transform) override {}
};

snd_hctl_t* g_hardware_controls = nullptr;
snd_ctl_elem_id_t* g_rate_offset_id = nullptr;
snd_ctl_elem_value_t* g_rate_offset_ppm = nullptr;
snd_hctl_elem_t* g_rate_offset_element = nullptr;

void InitializeAlsaControls() {
  RETURN_ON_ALSA_ERROR(snd_ctl_elem_id_malloc, &g_rate_offset_id);
  RETURN_ON_ALSA_ERROR(snd_ctl_elem_value_malloc, &g_rate_offset_ppm);

  std::string alsa_device_name = kDefaultPcmDevice;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAlsaOutputDevice)) {
    alsa_device_name =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kAlsaOutputDevice);
  }
  RETURN_ON_ALSA_ERROR(snd_hctl_open, &g_hardware_controls,
                       alsa_device_name.c_str(), kSoundControlBlockingMode);
  RETURN_ON_ALSA_ERROR(snd_hctl_load, g_hardware_controls);
  snd_ctl_elem_id_set_interface(g_rate_offset_id, SND_CTL_ELEM_IFACE_PCM);
  snd_ctl_elem_id_set_name(g_rate_offset_id, kRateOffsetInterfaceName);
  g_rate_offset_element =
      snd_hctl_find_elem(g_hardware_controls, g_rate_offset_id);
  if (g_rate_offset_element) {
    snd_ctl_elem_value_set_id(g_rate_offset_ppm, g_rate_offset_id);
  } else {
    LOG(ERROR) << "snd_hctl_find_elem failed to find the rate offset element.";
  }
}

DefaultVideoPlane* g_video_plane = nullptr;

}  // namespace

void CastMediaShlib::Initialize(const std::vector<std::string>& argv) {
  InitializeAlsaControls();
  ::media::InitializeMediaLibrary();
}

void CastMediaShlib::Finalize() {
  if (g_hardware_controls)
    snd_hctl_close(g_hardware_controls);
  snd_ctl_elem_value_free(g_rate_offset_ppm);
  snd_ctl_elem_id_free(g_rate_offset_id);

  g_hardware_controls = nullptr;
  g_rate_offset_id = nullptr;
  g_rate_offset_ppm = nullptr;
  g_rate_offset_element = nullptr;

  delete g_video_plane;
  g_video_plane = nullptr;
}

VideoPlane* CastMediaShlib::GetVideoPlane() {
  if (!g_video_plane) {
    g_video_plane = new DefaultVideoPlane();
  }
  return g_video_plane;
}

MediaPipelineBackend* CastMediaShlib::CreateMediaPipelineBackend(
    const MediaPipelineDeviceParams& params) {
  return new MediaPipelineBackendForMixer(params);
}

double CastMediaShlib::GetMediaClockRate() {
  int ppm = 0;
  if (!g_rate_offset_element) {
    LOG(INFO) << "g_rate_offset_element is null, ALSA rate offset control will "
                 "not be possible.";
    return kOneMhzReference;
  }
  snd_ctl_elem_value_t* rate_offset_ppm;
  snd_ctl_elem_value_alloca(&rate_offset_ppm);
  int err = snd_hctl_elem_read(g_rate_offset_element, rate_offset_ppm);
  if (err < 0) {
    LOG(ERROR) << "snd_htcl_elem_read error: " << snd_strerror(err);
    return kOneMhzReference;
  }
  ppm = snd_ctl_elem_value_get_integer(rate_offset_ppm, 0);
  return kOneMhzReference + ppm;
}

double CastMediaShlib::MediaClockRatePrecision() {
  return kGranularityHz;
}

void CastMediaShlib::MediaClockRateRange(double* minimum_rate,
                                         double* maximum_rate) {
  DCHECK(minimum_rate);
  DCHECK(maximum_rate);

  *minimum_rate = kOneMhzReference - kMaxAdjustmentHz;
  *maximum_rate = kOneMhzReference + kMaxAdjustmentHz;
}

bool CastMediaShlib::SetMediaClockRate(double new_rate) {
  int new_ppm = new_rate - kOneMhzReference;
  if (!g_rate_offset_element) {
    LOG(INFO) << "g_rate_offset_element is null, ALSA rate offset control will "
                 "not be possible.";
    return false;
  }
  snd_ctl_elem_value_t* rate_offset_ppm;
  snd_ctl_elem_value_alloca(&rate_offset_ppm);
  snd_ctl_elem_value_set_integer(rate_offset_ppm, 0, new_ppm);
  int err = snd_hctl_elem_write(g_rate_offset_element, rate_offset_ppm);
  if (err < 0) {
    LOG(ERROR) << "snd_htcl_elem_write error: " << snd_strerror(err);
    return false;
  }
  return true;
}

bool CastMediaShlib::SupportsMediaClockRateChange() {
  return g_rate_offset_element != nullptr;
}

}  // namespace media
}  // namespace chromecast
