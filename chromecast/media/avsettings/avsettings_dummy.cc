// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/avsettings/avsettings_dummy.h"

namespace chromecast {

AvSettingsDummy::AvSettingsDummy() : delegate_(nullptr) {}

AvSettingsDummy::~AvSettingsDummy() = default;

void AvSettingsDummy::Initialize(Delegate* delegate) {
  delegate_ = delegate;
}

void AvSettingsDummy::Finalize() {
  delegate_ = nullptr;
}

AvSettings::ActiveState AvSettingsDummy::GetActiveState() {
  return ActiveState::UNKNOWN;
}

bool AvSettingsDummy::TurnActive(bool switch_to_cast) {
  return false;
}

bool AvSettingsDummy::TurnStandby() {
  return false;
}

bool AvSettingsDummy::KeepSystemAwake(int time_millis) {
  return false;
}

AvSettings::AudioVolumeControlType
AvSettingsDummy::GetAudioVolumeControlType() {
  return MASTER_VOLUME;
}

bool AvSettingsDummy::GetAudioVolumeStepInterval(float* step_interval) {
  return false;  // Use default intervals per control type
}

int AvSettingsDummy::GetAudioCodecsSupported() {
  return 0;
}

int AvSettingsDummy::GetMaxAudioChannels(AudioCodec codec) {
  return 0;
}

bool AvSettingsDummy::GetScreenResolution(int* width, int* height) {
  return false;
}

int AvSettingsDummy::GetHDCPVersion() {
  return 0;
}

int AvSettingsDummy::GetSupportedEotfs() {
  return 0;
}

int AvSettingsDummy::GetDolbyVisionFlags() {
  return 0;
}

int AvSettingsDummy::GetScreenWidthMm() {
  return 0;
}

int AvSettingsDummy::GetScreenHeightMm() {
  return 0;
}

bool AvSettingsDummy::GetOutputRestrictions(OutputRestrictions* restrictions) {
  return false;
}

void AvSettingsDummy::ApplyOutputRestrictions(
    const OutputRestrictions& restrictions) {}

AvSettings::WakeOnCastStatus AvSettingsDummy::GetWakeOnCastStatus() {
  return WAKE_ON_CAST_NOT_SUPPORTED;
}

bool AvSettingsDummy::EnableWakeOnCast(bool enabled) {
  return false;
}

AvSettings::HdrOutputType AvSettingsDummy::GetHdrOutputType() {
  return HDR_OUTPUT_SDR;
}

bool AvSettingsDummy::SetHdmiVideoMode(bool allow_4k,
                                       int optimize_for_fps,
                                       AvSettings::HdrOutputType output_type) {
  return false;
}

bool AvSettingsDummy::IsHdrOutputSupportedByCurrentHdmiVideoMode(
    HdrOutputType output_type) {
  return false;
}

}  // namespace chromecast
