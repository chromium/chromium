// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_devices_pref_handler_stub.h"

#include "base/containers/contains.h"
#include "chromeos/ash/components/audio/audio_device.h"

namespace ash {

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
    if (device->is_input) {
      return 50.0;
    }
    return 75.0;
  }
  return audio_device_volume_gain_map_[device->stable_device_id];
}

void AudioDevicesPrefHandlerStub::SetVolumeGainValue(const AudioDevice& device,
                                                     double value) {
  audio_device_volume_gain_map_[device.stable_device_id] = value;
}

bool AudioDevicesPrefHandlerStub::GetMuteValue(const AudioDevice& device) {
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
  if (!base::Contains(audio_device_state_map_, device.stable_device_id)) {
    return false;
  }
  *active = audio_device_state_map_[device.stable_device_id].active;
  *activate_by_user =
      audio_device_state_map_[device.stable_device_id].activate_by_user;
  return true;
}

void AudioDevicesPrefHandlerStub::SetUserPriorityHigherThan(
    const AudioDevice& target,
    const AudioDevice* base) {
  int t = GetUserPriority(target);
  int b = 0;
  if (base) {
    b = GetUserPriority(*base);
  }

  // Don't need to update the user priority of `target` if it's already has
  // higher priority than base.
  if (t > b) {
    return;
  }

  if (t != kUserPriorityNone) {
    // before: [. . . t - - - b . . .]
    // after:  [. . . - - - b t . . .]
    for (auto& it : user_priority_map_) {
      if (it.second > t && it.second <= b) {
        user_priority_map_[it.first] -= 1;
      }
    }
    user_priority_map_[target.stable_device_id] = b;
  } else {
    // before: [. . . b + + +]
    // after : [. . . b t + + +]
    for (auto& it : user_priority_map_) {
      DCHECK(it.second > 0);
      if (it.second > b) {
        user_priority_map_[it.first] += 1;
      }
    }
    user_priority_map_[target.stable_device_id] = b + 1;
  }
}

int AudioDevicesPrefHandlerStub::GetUserPriority(const AudioDevice& device) {
  if (base::Contains(user_priority_map_, device.stable_device_id)) {
    return user_priority_map_[device.stable_device_id];
  }
  return kUserPriorityNone;
}

void AudioDevicesPrefHandlerStub::DropLeastRecentlySeenDevices(
    const std::vector<AudioDevice>& connected_devices,
    size_t keep_devices) {}

bool AudioDevicesPrefHandlerStub::GetNoiseCancellationState() {
  return noise_cancellation_state_;
}

void AudioDevicesPrefHandlerStub::SetNoiseCancellationState(
    bool noise_cancellation_state) {
  noise_cancellation_state_ = noise_cancellation_state;
}

bool AudioDevicesPrefHandlerStub::GetAudioOutputAllowedValue() const {
  return is_audio_output_allowed_;
}

void AudioDevicesPrefHandlerStub::SetAudioOutputAllowedValue(
    bool is_audio_output_allowed) {
  is_audio_output_allowed_ = is_audio_output_allowed;
  for (auto& observer : observers_) {
    observer.OnAudioPolicyPrefChanged();
  }
}

void AudioDevicesPrefHandlerStub::AddAudioPrefObserver(
    AudioPrefObserver* observer) {
  observers_.AddObserver(observer);
}

void AudioDevicesPrefHandlerStub::RemoveAudioPrefObserver(
    AudioPrefObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool AudioDevicesPrefHandlerStub::GetForceRespectUiGainsState() {
  return force_respect_ui_gains_;
}

void AudioDevicesPrefHandlerStub::SetForceRespectUiGainsState(
    bool force_respect_ui_gains) {
  force_respect_ui_gains_ = force_respect_ui_gains;
}

}  // namespace ash
