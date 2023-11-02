// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_buildflags.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/media_capabilities_shlib.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {

void CastMediaShlib::Initialize(const std::vector<std::string>& argv) {}

void CastMediaShlib::Finalize() {}

VideoPlane* CastMediaShlib::GetVideoPlane() {
  return nullptr;
}

MediaPipelineBackend* CastMediaShlib::CreateMediaPipelineBackend(
    const MediaPipelineDeviceParams& params) {
  return nullptr;
}

double CastMediaShlib::GetMediaClockRate() {
  return 0.0;
}

double CastMediaShlib::MediaClockRatePrecision() {
  return 0.0;
}

void CastMediaShlib::MediaClockRateRange(double* minimum_rate,
                                         double* maximum_rate) {}

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

#if BUILDFLAG(VOLUME_CONTROL_IN_MEDIA_SHLIB)

void VolumeControl::Initialize(const std::vector<std::string>& argv) {}
void VolumeControl::Finalize() {}
void VolumeControl::AddVolumeObserver(VolumeObserver* observer) {}
void VolumeControl::RemoveVolumeObserver(VolumeObserver* observer) {}

float VolumeControl::GetVolume(AudioContentType type) {
  return 0.0f;
}

void VolumeControl::SetVolume(VolumeChangeSource source,
                              AudioContentType type,
                              float level) {}

bool VolumeControl::IsMuted(AudioContentType type) {
  return false;
}

void VolumeControl::SetMuted(VolumeChangeSource source,
                             AudioContentType type,
                             bool muted) {}

void VolumeControl::SetOutputLimit(AudioContentType type, float limit) {}

float VolumeControl::VolumeToDbFS(float volume) {
  return 0.0f;
}

float VolumeControl::DbFSToVolume(float db) {
  return 0.0f;
}

void VolumeControl::SetPowerSaveMode(bool power_save_on) {}

#endif  // BUILDFLAG(VOLUME_CONTROL_IN_MEDIA_SHLIB)

}  // namespace media
}  // namespace chromecast
