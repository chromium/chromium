// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_host_impl.h"

#include "base/check.h"
#include "base/optional.h"
#include "chromeos/services/assistant/platform/audio_devices.h"
#include "chromeos/services/assistant/proxy/audio_input_bindings.h"
#include "chromeos/services/assistant/public/cpp/assistant_client.h"
#include "chromeos/services/assistant/public/cpp/features.h"

namespace chromeos {
namespace assistant {

namespace {

using MojomLidState = chromeos::libassistant::mojom::LidState;

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

// Delegate that will fetch an audio stream factory from the |AssistantClient|.
class AudioStreamFactoryDelegateImpl
    : public chromeos::libassistant::mojom::AudioStreamFactoryDelegate {
 public:
  explicit AudioStreamFactoryDelegateImpl(
      mojo::PendingReceiver<
          chromeos::libassistant::mojom::AudioStreamFactoryDelegate>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  AudioStreamFactoryDelegateImpl(const AudioStreamFactoryDelegateImpl&) =
      delete;
  AudioStreamFactoryDelegateImpl& operator=(
      const AudioStreamFactoryDelegateImpl&) = delete;
  ~AudioStreamFactoryDelegateImpl() override = default;

  void GetAudioStreamFactory(GetAudioStreamFactoryCallback callback) override {
    mojo::PendingRemote<audio::mojom::StreamFactory> result;
    AssistantClient::Get()->RequestAudioStreamFactory(
        result.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(result));
  }

 private:
  mojo::Receiver<chromeos::libassistant::mojom::AudioStreamFactoryDelegate>
      receiver_;
};

chromeos::assistant::AudioInputHostImpl::AudioInputHostImpl(
    AudioInputBindings bindings,
    CrasAudioHandler* cras_audio_handler,
    chromeos::PowerManagerClient* power_manager_client,
    const std::string& locale)
    : remote_(std::move(bindings.pending_audio_input_controller_remote)),
      power_manager_client_(power_manager_client),
      power_manager_client_observer_(this),
      audio_devices_(cras_audio_handler, locale),
      audio_stream_factory_delegate_(
          std::make_unique<AudioStreamFactoryDelegateImpl>(std::move(
              bindings.pending_audio_stream_factory_delegate_receiver))) {
  DCHECK(power_manager_client_);

  audio_devices_observation_.Observe(&audio_devices_);
  power_manager_client_observer_.Observe(power_manager_client_);
  power_manager_client_->GetSwitchStates(
      base::BindOnce(&AudioInputHostImpl::OnInitialLidStateReceived,
                     weak_factory_.GetWeakPtr()));
}

AudioInputHostImpl::~AudioInputHostImpl() = default;

void AudioInputHostImpl::SetMicState(bool mic_open) {
  remote_->SetMicOpen(mic_open);
}

void AudioInputHostImpl::SetDeviceId(
    const base::Optional<std::string>& device_id) {
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

void AudioInputHostImpl::OnConversationTurnFinished() {
  remote_->OnConversationTurnFinished();
}

void AudioInputHostImpl::OnHotwordEnabled(bool enable) {
  remote_->SetHotwordEnabled(enable);
}

void AudioInputHostImpl::SetHotwordDeviceId(
    const base::Optional<std::string>& device_id) {
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
    base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states) {
  if (switch_states.has_value())
    remote_->SetLidState(ConvertLidState(switch_states->lid_state));
}

}  // namespace assistant
}  // namespace chromeos
