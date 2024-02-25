// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/audio_input_controller.h"

#include "base/notreached.h"

namespace ash::libassistant {

AudioInputController::AudioInputController() = default;

AudioInputController::~AudioInputController() = default;

void AudioInputController::Bind(
    mojo::PendingReceiver<mojom::AudioInputController> receiver,
    mojom::PlatformDelegate* platform_delegate) {
  receiver_.Bind(std::move(receiver));
  audio_input().Initialize(platform_delegate);
}

void AudioInputController::SetMicOpen(bool mic_open) {
  audio_input().SetMicState(mic_open);
}

void AudioInputController::SetHotwordEnabled(bool enable) {
  audio_input().OnHotwordEnabled(enable);
}

void AudioInputController::SetDeviceId(
    const std::optional<std::string>& device_id) {
  DCHECK(device_id != "");
  audio_input().SetDeviceId(device_id);
}

void AudioInputController::SetHotwordDeviceId(
    const std::optional<std::string>& device_id) {
  DCHECK(device_id != "");
  audio_input().SetHotwordDeviceId(device_id);
}

void AudioInputController::SetLidState(mojom::LidState new_state) {
  audio_input().OnLidStateChanged(new_state);
}

void AudioInputController::OnConversationTurnStarted() {
  audio_input().OnConversationTurnStarted();
}

void AudioInputController::OnInteractionFinished(Resolution resolution) {
  // TODO(b/179924068): Find a better way to handle the edge cases.
  if (resolution != Resolution::NORMAL_WITH_FOLLOW_ON &&
      resolution != Resolution::CANCELLED &&
      resolution != Resolution::BARGE_IN) {
    SetMicOpen(false);
  }

  audio_input().OnConversationTurnFinished();
}

AudioInputImpl& AudioInputController::audio_input() {
  return audio_input_provider().GetAudioInput();
}

}  // namespace ash::libassistant
