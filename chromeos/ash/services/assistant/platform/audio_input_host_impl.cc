// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/platform/audio_input_host_impl.h"

#include <optional>

#include "base/check.h"
#include "chromeos/ash/services/assistant/platform/audio_devices.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"

namespace ash::assistant {

namespace {

using MojomLidState = libassistant::mojom::LidState;

MojomLidState ConvertLidState(chromeos::PowerManagerClient::LidState state) {
  switch (state) {
    case chromeos::PowerManagerClient::LidState::CLOSED:
      return MojomLidState::kClosed;
    case chromeos::PowerManagerClient::LidState::OPEN:
      return MojomLidState::kOpen;
    case chromeos::PowerManagerClient::LidState::NOT_PRESENT:
      // If there is no lid, it can't be closed.
      return MojomLidState::kOpen;
  }
}

}  // namespace

AudioInputHostImpl::AudioInputHostImpl(
    mojo::PendingRemote<libassistant::mojom::AudioInputController>
        pending_remote,
    CrasAudioHandler* cras_audio_handler,
    chromeos::PowerManagerClient* power_manager_client,
    const std::string& locale)
    : remote_(std::move(pending_remote)),
      power_manager_client_(power_manager_client),
      power_manager_client_observer_(this),
      audio_devices_(cras_audio_handler, locale) {
  DCHECK(power_manager_client_);

  audio_devices_observation_.Observe(&audio_devices_);
  power_manager_client_observer_.Observe(power_manager_client_.get());
  power_manager_client_->GetSwitchStates(
      base::BindOnce(&AudioInputHostImpl::OnInitialLidStateReceived,
                     weak_factory_.GetWeakPtr()));
}

AudioInputHostImpl::~AudioInputHostImpl() = default;

void AudioInputHostImpl::SetMicState(bool mic_open) {
  remote_->SetMicOpen(mic_open);
}

void AudioInputHostImpl::SetDeviceId(
    const std::optional<std::string>& device_id) {
  remote_->SetDeviceId(device_id);
}

void AudioInputHostImpl::OnConversationTurnStarted() {
  remote_->OnConversationTurnStarted();
  // Inform power manager of a wake notification when Libassistant
  // recognized hotword and started a conversation. We intentionally
  // avoid using |NotifyUserActivity| because it is not suitable for
  // this case according to the Platform team.
  power_manager_client_->NotifyWakeNotification();
}

void AudioInputHostImpl::OnHotwordEnabled(bool enable) {
  remote_->SetHotwordEnabled(enable);
}

void AudioInputHostImpl::SetHotwordDeviceId(
    const std::optional<std::string>& device_id) {
  remote_->SetHotwordDeviceId(device_id);
}

void AudioInputHostImpl::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    base::TimeTicks timestamp) {
  // Lid switch event still gets fired during system suspend, which enables
  // us to stop DSP recording correctly when user closes lid after the device
  // goes to sleep.
  remote_->SetLidState(ConvertLidState(state));
}

void AudioInputHostImpl::OnInitialLidStateReceived(
    std::optional<chromeos::PowerManagerClient::SwitchStates> switch_states) {
  if (switch_states.has_value())
    remote_->SetLidState(ConvertLidState(switch_states->lid_state));
}

}  // namespace ash::assistant
