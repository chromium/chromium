// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/fuchsia/fuchsia_volume_control.h"

#include "base/notreached.h"

namespace chromecast {
namespace media {

// static
std::unique_ptr<SystemVolumeControl> SystemVolumeControl::Create(
    Delegate* delegate) {
  return std::make_unique<FuchsiaVolumeControl>();
}

FuchsiaVolumeControl::FuchsiaVolumeControl() {}
FuchsiaVolumeControl::~FuchsiaVolumeControl() {}

float FuchsiaVolumeControl::GetRoundtripVolume(float volume) {
  NOTIMPLEMENTED();
  return volume;
}

float FuchsiaVolumeControl::GetVolume() {
  NOTIMPLEMENTED();
  return 1.0;
}

void FuchsiaVolumeControl::SetVolume(float level) {
  NOTIMPLEMENTED();
}

bool FuchsiaVolumeControl::IsMuted() {
  NOTIMPLEMENTED();
  return false;
}

void FuchsiaVolumeControl::SetMuted(bool muted) {
  NOTIMPLEMENTED();
}

void FuchsiaVolumeControl::SetPowerSave(bool power_save_on) {
  NOTIMPLEMENTED();
}

void FuchsiaVolumeControl::SetLimit(float limit) {
  NOTIMPLEMENTED();
}

}  // namespace media
}  // namespace chromecast
