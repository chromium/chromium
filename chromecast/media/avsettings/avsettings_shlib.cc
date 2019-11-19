// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chromecast/media/avsettings/avsettings_buildflags.h"
#include "chromecast/media/avsettings/avsettings_dummy.h"
#include "chromecast/public/avsettings.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {

// static
AvSettings* AvSettingsShlib::Create(const std::vector<std::string>& argv) {
  return new AvSettingsDummy();
}

#if BUILDFLAG(VOLUME_CONTROL_IN_AVSETTINGS_SHLIB)
namespace media {

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

}  // namespace media
#endif  // BUILDFLAG(VOLUME_CONTROL_IN_AVSETTINGS_SHLIB)

}  // namespace chromecast
