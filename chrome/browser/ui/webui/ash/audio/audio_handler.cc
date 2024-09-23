// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <utility>

#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/ash/audio/audio_handler.h"

namespace ash {

AudioHandler::AudioHandler(
    mojo::PendingReceiver<audio::mojom::PageHandler> receiver,
    mojo::PendingRemote<audio::mojom::Page> page)
    : page_(std::move(page)), receiver_(this, std::move(receiver)) {
  observation_.Observe(CrasAudioHandler::Get());
}

AudioHandler::~AudioHandler() = default;

void AudioHandler::GetAudioDeviceInfo() {
  UpdateAudioDeviceInfo();
}

void AudioHandler::GetActiveOutputDeviceName(
    audio::mojom::PageHandler::GetActiveOutputDeviceNameCallback callback) {
  const uint64_t output_id =
      ash::CrasAudioHandler::Get()->GetPrimaryActiveOutputNode();
  const ash::AudioDevice* output_device =
      ash::CrasAudioHandler::Get()->GetDeviceFromId(output_id);
  if (output_device) {
    std::move(callback).Run(output_device->display_name);
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void AudioHandler::GetActiveInputDeviceName(
    audio::mojom::PageHandler::GetActiveInputDeviceNameCallback callback) {
  const uint64_t input_id =
      ash::CrasAudioHandler::Get()->GetPrimaryActiveInputNode();
  const ash::AudioDevice* input_device =
      ash::CrasAudioHandler::Get()->GetDeviceFromId(input_id);
  if (input_device) {
    std::move(callback).Run(input_device->display_name);
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void AudioHandler::OpenFeedbackDialog() {
  chrome::OpenFeedbackDialog(chrome::FindBrowserWithActiveWindow(),
                             feedback::kFeedbackSourceMdSettingsAboutPage);
}

void AudioHandler::OnAudioNodesChanged() {
  UpdateAudioDeviceInfo();
}

void AudioHandler::OnOutputNodeVolumeChanged(uint64_t node_id, int volume) {
  page_->UpdateDeviceVolume(node_id, volume);
}

void AudioHandler::OnInputNodeGainChanged(uint64_t node_id, int gain) {
  page_->UpdateDeviceVolume(node_id, gain);
}

void AudioHandler::OnOutputMuteChanged(bool mute) {
  const uint64_t output_id =
      ash::CrasAudioHandler::Get()->GetPrimaryActiveOutputNode();
  page_->UpdateDeviceMute(output_id, mute);
}

void AudioHandler::OnInputMuteChanged(
    bool mute,
    CrasAudioHandler::InputMuteChangeMethod method) {
  const uint64_t input_id =
      ash::CrasAudioHandler::Get()->GetPrimaryActiveInputNode();
  page_->UpdateDeviceMute(input_id, mute);
}

void AudioHandler::OnInputMutedByMicrophoneMuteSwitchChanged(bool mute) {
  const uint64_t input_id =
      ash::CrasAudioHandler::Get()->GetPrimaryActiveInputNode();
  page_->UpdateDeviceMute(input_id, mute);
}

void AudioHandler::OnActiveOutputNodeChanged() {
  UpdateAudioDeviceInfo();
}

void AudioHandler::OnActiveInputNodeChanged() {
  UpdateAudioDeviceInfo();
}

void AudioHandler::UpdateAudioDeviceInfo() {
  ash::AudioDeviceList devices;
  ash::CrasAudioHandler::Get()->GetAudioDevices(&devices);
  base::flat_map<uint64_t, audio::mojom::DeviceDataPtr> device_map;

  for (ash::AudioDeviceList::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    device_map[it->id] = CreateDeviceData(&(*it));
  }
  page_->UpdateDeviceInfo(std::move(device_map));
}

audio::mojom::DeviceDataPtr AudioHandler::CreateDeviceData(
    const ash::AudioDevice* item) const {
  auto device_data = audio::mojom::DeviceData::New();

  device_data->type = item->GetTypeString(item->type);
  device_data->id = item->id;
  device_data->display_name = item->display_name;
  device_data->is_active = item->active;
  device_data->is_input = item->is_input;
  std::tie(device_data->volume_gain_percent, device_data->is_muted) =
      GetDeviceVolGain(item->id, item->is_input);
  return device_data;
}

std::tuple<int, bool> AudioHandler::GetDeviceVolGain(uint64_t id,
                                                     bool is_input) const {
  if (is_input) {
    return std::make_tuple(
        ash::CrasAudioHandler::Get()->GetInputGainPercentForDevice(id),
        ash::CrasAudioHandler::Get()->IsInputMutedForDevice(id));
  } else {
    return std::make_tuple(
        ash::CrasAudioHandler::Get()->GetOutputVolumePercentForDevice(id),
        ash::CrasAudioHandler::Get()->IsOutputMutedForDevice(id));
  }
}

}  // namespace ash
