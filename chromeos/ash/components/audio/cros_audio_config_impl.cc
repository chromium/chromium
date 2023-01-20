// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/cros_audio_config_impl.h"

#include "base/logging.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash::audio_config {

namespace {

constexpr int kDefaultInternalMicId = 0;
constexpr char kStubInternalMicDisplayName[] = "Internal Mic";

// Creates an inactive input device with default property configuration.
AudioDevice CreateStubInternalMic() {
  AudioDevice internal_mic;
  internal_mic.id = kDefaultInternalMicId;
  internal_mic.is_input = true;
  // TODO(b/260277007): Replace with lookup for localized device name.
  internal_mic.display_name = kStubInternalMicDisplayName;
  internal_mic.stable_device_id_version = 2;
  internal_mic.type = AudioDeviceType::kInternalMic;
  internal_mic.active = false;
  return internal_mic;
}

// Updates active and id properties on stub `internal_mic` based on provided
// front or rear device.
void UpdateInternalMicBasedOnAudioDevice(AudioDevice& internal_mic,
                                         const AudioDevice& device) {
  DCHECK(device.is_input && (device.type == AudioDeviceType::kFrontMic ||
                             device.type == AudioDeviceType::kRearMic));

  // Update internal_mic id if it has not been set or if the incoming device is
  // active.
  if (internal_mic.id == kDefaultInternalMicId || device.active) {
    internal_mic.id = device.id;
  }

  // Update active
  if (device.active) {
    internal_mic.active = true;
  }

  // TODO(b/260277007): Add noise cancellation to audio effects after
  // CrasAudioHandler noise cancellation refactor complete and property added
  // to mojo.
}

}  // namespace

mojom::AudioDeviceType ComputeDeviceType(const AudioDeviceType& device_type) {
  switch (device_type) {
    case AudioDeviceType::kHeadphone:
      return mojom::AudioDeviceType::kHeadphone;
    case AudioDeviceType::kMic:
      return mojom::AudioDeviceType::kMic;
    case AudioDeviceType::kUsb:
      return mojom::AudioDeviceType::kUsb;
    case AudioDeviceType::kBluetooth:
      return mojom::AudioDeviceType::kBluetooth;
    case AudioDeviceType::kBluetoothNbMic:
      return mojom::AudioDeviceType::kBluetoothNbMic;
    case AudioDeviceType::kHdmi:
      return mojom::AudioDeviceType::kHdmi;
    case AudioDeviceType::kInternalSpeaker:
      return mojom::AudioDeviceType::kInternalSpeaker;
    case AudioDeviceType::kInternalMic:
      return mojom::AudioDeviceType::kInternalMic;
    case AudioDeviceType::kFrontMic:
      return mojom::AudioDeviceType::kFrontMic;
    case AudioDeviceType::kRearMic:
      return mojom::AudioDeviceType::kRearMic;
    case AudioDeviceType::kKeyboardMic:
      return mojom::AudioDeviceType::kKeyboardMic;
    case AudioDeviceType::kHotword:
      return mojom::AudioDeviceType::kHotword;
    case AudioDeviceType::kPostDspLoopback:
      return mojom::AudioDeviceType::kPostDspLoopback;
    case AudioDeviceType::kPostMixLoopback:
      return mojom::AudioDeviceType::kPostMixLoopback;
    case AudioDeviceType::kLineout:
      return mojom::AudioDeviceType::kLineout;
    case AudioDeviceType::kAlsaLoopback:
      return mojom::AudioDeviceType::kAlsaLoopback;
    case AudioDeviceType::kOther:
      return mojom::AudioDeviceType::kOther;
  };
}

mojom::AudioDevicePtr GenerateMojoAudioDevice(const AudioDevice& device) {
  mojom::AudioDevicePtr mojo_device = mojom::AudioDevice::New();
  mojo_device->id = device.id;
  mojo_device->display_name = device.display_name;
  mojo_device->is_active = device.active;
  mojo_device->device_type = ComputeDeviceType(device.type);
  return mojo_device;
}

CrosAudioConfigImpl::CrosAudioConfigImpl() {
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

CrosAudioConfigImpl::~CrosAudioConfigImpl() {
  if (CrasAudioHandler::Get())
    CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

uint8_t CrosAudioConfigImpl::GetOutputVolumePercent() const {
  return CrasAudioHandler::Get()->GetOutputVolumePercent();
}

uint8_t CrosAudioConfigImpl::GetInputGainPercent() const {
  return CrasAudioHandler::Get()->GetInputGainPercent();
}

mojom::MuteState CrosAudioConfigImpl::GetOutputMuteState() const {
  // TODO(crbug.com/1092970): Add kMutedExternally.
  if (CrasAudioHandler::Get()->IsOutputMutedByPolicy())
    return mojom::MuteState::kMutedByPolicy;

  if (CrasAudioHandler::Get()->IsOutputMuted())
    return mojom::MuteState::kMutedByUser;

  return mojom::MuteState::kNotMuted;
}

void CrosAudioConfigImpl::GetAudioDevices(
    std::vector<mojom::AudioDevicePtr>* output_devices_out,
    std::vector<mojom::AudioDevicePtr>* input_devices_out) const {
  DCHECK(output_devices_out);
  DCHECK(input_devices_out);
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  AudioDeviceList audio_devices_list;
  audio_handler->GetAudioDevices(&audio_devices_list);

  // For device that has dual internal mics, a new AudioDevice will be created
  // to show only one slider for both the internal mics, and the new AudioDevice
  // has a new id that doesn't match either the first or active internal mic.
  bool has_dual_internal_mic = audio_handler->HasDualInternalMic();
  AudioDevice internal_mic = CreateStubInternalMic();

  for (const auto& device : audio_devices_list) {
    if (!device.is_for_simple_usage()) {
      continue;
    }

    // If dual mics is enabled and device is front or rear mic then use device
    // to set common properties on stub internal_mic and skip
    // adding to list of input devices.
    if (has_dual_internal_mic && audio_handler->IsFrontOrRearMic(device)) {
      UpdateInternalMicBasedOnAudioDevice(internal_mic, device);
      continue;
    }

    if (device.is_input) {
      input_devices_out->push_back(GenerateMojoAudioDevice(device));
    } else {
      output_devices_out->push_back(GenerateMojoAudioDevice(device));
    }
  }

  // Add stub internal mic in place of front and rear mic devices.
  if (has_dual_internal_mic) {
    DCHECK(internal_mic.id != kDefaultInternalMicId);
    input_devices_out->push_back(GenerateMojoAudioDevice(internal_mic));
  }
}

mojom::MuteState CrosAudioConfigImpl::GetInputMuteState() const {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  if (audio_handler->input_muted_by_microphone_mute_switch() &&
      audio_handler->IsInputMuted()) {
    return mojom::MuteState::kMutedExternally;
  }

  if (audio_handler->IsInputMuted()) {
    return mojom::MuteState::kMutedByUser;
  }

  return mojom::MuteState::kNotMuted;
}

void CrosAudioConfigImpl::SetOutputMuted(bool muted) {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  if (audio_handler->IsOutputMutedByPolicy()) {
    return;
  }

  audio_handler->SetOutputMute(muted);
}

void CrosAudioConfigImpl::SetOutputVolumePercent(int8_t volume) {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  audio_handler->SetOutputVolumePercent(volume);

  // If the volume is above certain level and it's muted, it should be unmuted.
  if (audio_handler->IsOutputMuted() &&
      volume > audio_handler->GetOutputDefaultVolumeMuteThreshold()) {
    audio_handler->SetOutputMute(false);
  }
}

void CrosAudioConfigImpl::SetInputGainPercent(uint8_t gain) {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  audio_handler->SetInputGainPercent(gain);

  // Unmute if muted.
  if (audio_handler->IsInputMuted()) {
    audio_handler->SetInputMute(
        false, CrasAudioHandler::InputMuteChangeMethod::kOther);
  }
}

void CrosAudioConfigImpl::SetActiveDevice(uint64_t device_id) {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  const AudioDevice* next_active_device =
      audio_handler->GetDeviceFromId(device_id);

  if (!next_active_device) {
    LOG(ERROR) << "SetActiveDevice: Cannot find device id="
               << "0x" << std::hex << device_id;
    return;
  }

  // When device has dual mics the `GetAudioDevices` represents front and rear
  // mic as a single device. To set active internal mic correctly
  // `SwitchToFrontOrRearMic` needs to be called.
  if (audio_handler->HasDualInternalMic() &&
      audio_handler->IsFrontOrRearMic(*next_active_device)) {
    audio_handler->SwitchToFrontOrRearMic();
  } else {
    audio_handler->SwitchToDevice(
        *next_active_device, /*notify=*/true,
        CrasAudioHandler::DeviceActivateType::ACTIVATE_BY_USER);
  }
}

void CrosAudioConfigImpl::SetInputMuted(bool muted) {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  audio_handler->SetMuteForDevice(audio_handler->GetPrimaryActiveInputNode(),
                                  muted);
}

void CrosAudioConfigImpl::OnOutputNodeVolumeChanged(uint64_t node_id,
                                                    int volume) {
  NotifyObserversAudioSystemPropertiesChanged();
}

void CrosAudioConfigImpl::OnInputNodeGainChanged(uint64_t node_id, int gain) {
  NotifyObserversAudioSystemPropertiesChanged();
}

void CrosAudioConfigImpl::OnOutputMuteChanged(bool mute_on) {
  NotifyObserversAudioSystemPropertiesChanged();
}

void CrosAudioConfigImpl::OnAudioNodesChanged() {
  NotifyObserversAudioSystemPropertiesChanged();
}

void CrosAudioConfigImpl::OnActiveOutputNodeChanged() {
  NotifyObserversAudioSystemPropertiesChanged();
}

void CrosAudioConfigImpl::OnActiveInputNodeChanged() {
  NotifyObserversAudioSystemPropertiesChanged();
}

void CrosAudioConfigImpl::OnInputMuteChanged(
    bool mute_on,
    CrasAudioHandler::InputMuteChangeMethod method) {
  NotifyObserversAudioSystemPropertiesChanged();
}

void CrosAudioConfigImpl::OnInputMutedByMicrophoneMuteSwitchChanged(
    bool muted) {
  NotifyObserversAudioSystemPropertiesChanged();
}

}  // namespace ash::audio_config
