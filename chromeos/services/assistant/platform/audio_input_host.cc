// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_host.h"

#include "base/check.h"
#include "base/optional.h"
#include "chromeos/services/assistant/platform/audio_devices.h"
#include "chromeos/services/assistant/platform/audio_input_impl.h"
#include "chromeos/services/assistant/public/cpp/features.h"

namespace chromeos {
namespace assistant {

namespace {

AudioInputImpl::LidState ConvertLidState(
    chromeos::PowerManagerClient::LidState state) {
  switch (state) {
    case chromeos::PowerManagerClient::LidState::CLOSED:
      return AudioInputImpl::LidState::kClosed;
    case chromeos::PowerManagerClient::LidState::OPEN:
      return AudioInputImpl::LidState::kOpen;
    case chromeos::PowerManagerClient::LidState::NOT_PRESENT:
      // If there is no lid, it can't be closed.
      return AudioInputImpl::LidState::kOpen;
  }
}

}  // namespace

chromeos::assistant::AudioInputHost::AudioInputHost(
    AudioInputImpl* audio_input,
    CrasAudioHandler* cras_audio_handler,
    chromeos::PowerManagerClient* power_manager_client,
    const std::string& locale)
    : audio_input_(audio_input),
      power_manager_client_(power_manager_client),
      power_manager_client_observer_(this),
      audio_devices_(cras_audio_handler, locale) {
  DCHECK(audio_input_);
  DCHECK(power_manager_client_);

  audio_devices_observation_.Observe(&audio_devices_);
  power_manager_client_observer_.Observe(power_manager_client);
  power_manager_client->GetSwitchStates(base::BindOnce(
      &AudioInputHost::OnInitialLidStateReceived, weak_factory_.GetWeakPtr()));
}

AudioInputHost::~AudioInputHost() = default;

void AudioInputHost::SetMicState(bool mic_open) {
  audio_input_->SetMicState(mic_open);
}

void AudioInputHost::SetDeviceId(const base::Optional<std::string>& device_id) {
  audio_input_->SetDeviceId(device_id.value_or(""));
}

void AudioInputHost::OnConversationTurnStarted() {
  audio_input_->OnConversationTurnStarted();
  // Inform power manager of a wake notification when Libassistant
  // recognized hotword and started a conversation. We intentionally
  // avoid using |NotifyUserActivity| because it is not suitable for
  // this case according to the Platform team.
  power_manager_client_->NotifyWakeNotification();
}

void AudioInputHost::OnConversationTurnFinished() {
  audio_input_->OnConversationTurnFinished();
}

void AudioInputHost::OnHotwordEnabled(bool enable) {
  audio_input_->OnHotwordEnabled(enable);
}

void AudioInputHost::SetHotwordDeviceId(
    const base::Optional<std::string>& device_id) {
  audio_input_->SetHotwordDeviceId(device_id.value_or(""));
}

void AudioInputHost::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    base::TimeTicks timestamp) {
  // Lid switch event still gets fired during system suspend, which enables
  // us to stop DSP recording correctly when user closes lid after the device
  // goes to sleep.
  audio_input_->OnLidStateChanged(ConvertLidState(state));
}

void AudioInputHost::OnInitialLidStateReceived(
    base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states) {
  if (switch_states.has_value())
    audio_input_->OnLidStateChanged(ConvertLidState(switch_states->lid_state));
}

}  // namespace assistant
}  // namespace chromeos
