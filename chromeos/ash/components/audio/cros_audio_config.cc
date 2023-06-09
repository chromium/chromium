// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/cros_audio_config.h"

#include <utility>

namespace ash::audio_config {

CrosAudioConfig::CrosAudioConfig() = default;
CrosAudioConfig::~CrosAudioConfig() = default;

void CrosAudioConfig::ObserveAudioSystemProperties(
    mojo::PendingRemote<mojom::AudioSystemPropertiesObserver> observer) {
  const mojo::RemoteSetElementId id = observers_.Add(std::move(observer));
  observers_.Get(id)->OnPropertiesUpdated(GetAudioSystemProperties());
}

mojom::AudioSystemPropertiesPtr CrosAudioConfig::GetAudioSystemProperties() {
  auto properties = mojom::AudioSystemProperties::New();
  properties->output_volume_percent = GetOutputVolumePercent();
  properties->input_gain_percent = GetInputGainPercent();
  properties->output_mute_state = GetOutputMuteState();
  properties->input_mute_state = GetInputMuteState();
  GetAudioDevices(&properties->output_devices, &properties->input_devices);
  return properties;
}

void CrosAudioConfig::BindPendingReceiver(
    mojo::PendingReceiver<mojom::CrosAudioConfig> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void CrosAudioConfig::NotifyObserversAudioSystemPropertiesChanged() {
  for (const auto& observer : observers_) {
    observer->OnPropertiesUpdated(GetAudioSystemProperties());
  }
}

}  // namespace ash::audio_config
