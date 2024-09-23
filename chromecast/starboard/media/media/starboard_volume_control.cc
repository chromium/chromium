// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// System volume control logic is handled by cast core, not the runtime, so
// these functions are stubbed out.

#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {

// static
void VolumeControl::Initialize(const std::vector<std::string>& argv) {}

// static
void VolumeControl::Finalize() {}

// static
void VolumeControl::AddVolumeObserver(VolumeObserver* observer) {}

// static
void VolumeControl::RemoveVolumeObserver(VolumeObserver* observer) {}

// static
float VolumeControl::GetVolume(AudioContentType type) {
  return 0.0f;
}

// static
void VolumeControl::SetVolume(media::VolumeChangeSource source,
                              AudioContentType type,
                              float level) {}

// static
bool VolumeControl::IsMuted(AudioContentType type) {
  return false;
}

// static
void VolumeControl::SetMuted(media::VolumeChangeSource source,
                             AudioContentType type,
                             bool muted) {}

// static
void VolumeControl::SetOutputLimit(AudioContentType type, float limit) {}

// static
float VolumeControl::VolumeToDbFS(float volume) {
  return 0.0f;
}

// static
float VolumeControl::DbFSToVolume(float db) {
  return 0.0f;
}

}  // namespace media
}  // namespace chromecast
