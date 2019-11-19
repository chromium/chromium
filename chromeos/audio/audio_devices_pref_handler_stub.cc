// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/audio_devices_pref_handler_stub.h"

#include "base/stl_util.h"
#include "chromeos/audio/audio_device.h"

namespace chromeos {

AudioDevicesPrefHandlerStub::AudioDevicesPrefHandlerStub() = default;

AudioDevicesPrefHandlerStub::~AudioDevicesPrefHandlerStub() = default;

double AudioDevicesPrefHandlerStub::GetOutputVolumeValue(
    const AudioDevice* device) {
  if (!device || !base::Contains(audio_device_volume_gain_map_,
                                 device->stable_device_id)) {
    return kDefaultOutputVolumePercent;
  }
  return audio_device_volume_gain_map_[device->stable_device_id];
}

double AudioDevicesPrefHandlerStub::GetInputGainValue(
    const AudioDevice* device) {
  // TODO(rkc): The default value for gain is wrong. http://crbug.com/442489
  if (!device || !base::Contains(audio_device_volume_gain_map_,
                                 device->stable_device_id)) {
    return 75.0;
  }
  return audio_device_volume_gain_map_[device->stable_device_id];
}

void AudioDevicesPrefHandlerStub::SetVolumeGainValue(const AudioDevice& device,
                                                     double value) {
  audio_device_volume_gain_map_[device.stable_device_id] = value;
}

bool AudioDevicesPrefHandlerStub::GetMuteValue(
    const AudioDevice& device) {
  return audio_device_mute_map_[device.stable_device_id];
}

void AudioDevicesPrefHandlerStub::SetMuteValue(const AudioDevice& device,
                                               bool mute_on) {
  audio_device_mute_map_[device.stable_device_id] = mute_on;
}

void AudioDevicesPrefHandlerStub::SetDeviceActive(const AudioDevice& device,
                                                  bool active,
                                                  bool activate_by_user) {
  DeviceState state;
  state.active = active;
  state.activate_by_user = activate_by_user;
  audio_device_state_map_[device.stable_device_id] = state;
}

bool AudioDevicesPrefHandlerStub::GetDeviceActive(const AudioDevice& device,
                                                  bool* active,
                                                  bool* activate_by_user) {
  if (audio_device_state_map_.find(device.stable_device_id) ==
      audio_device_state_map_.end()) {
    return false;
  }
  *active = audio_device_state_map_[device.stable_device_id].active;
  *activate_by_user =
      audio_device_state_map_[device.stable_device_id].activate_by_user;
  return true;
}

bool AudioDevicesPrefHandlerStub::GetAudioOutputAllowedValue() {
  return true;
}

void AudioDevicesPrefHandlerStub::AddAudioPrefObserver(
    AudioPrefObserver* observer) {
}

void AudioDevicesPrefHandlerStub::RemoveAudioPrefObserver(
    AudioPrefObserver* observer) {
}

}  // namespace chromeos

