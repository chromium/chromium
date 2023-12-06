// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_INPUT_CONTROLLER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_INPUT_CONTROLLER_H_

#include "chromeos/ash/services/libassistant/audio/audio_input_provider_impl.h"
#include "chromeos/ash/services/libassistant/conversation_state_listener_impl.h"
#include "chromeos/ash/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::libassistant {

// Implementation of |mojom::AudioInputController| that will forward all calls
// to a Libassistant V1 |assistant_client::AudioInputProvider| implementation.
class COMPONENT_EXPORT(LIBASSISTANT_SERVICE) AudioInputController
    : public mojom::AudioInputController {
 public:
  using Resolution = assistant_client::ConversationStateListener::Resolution;

  AudioInputController();
  AudioInputController(AudioInputController&) = delete;
  AudioInputController& operator=(AudioInputController&) = delete;
  ~AudioInputController() override;

  void Bind(mojo::PendingReceiver<mojom::AudioInputController> receiver,
            mojom::PlatformDelegate* platform_delegate);

  // mojom::AudioInputController implementation:
  void SetMicOpen(bool mic_open) override;
  void SetHotwordEnabled(bool enable) override;
  void SetDeviceId(const std::optional<std::string>& device_id) override;
  void SetHotwordDeviceId(const std::optional<std::string>& device_id) override;
  void SetLidState(mojom::LidState new_state) override;
  void OnConversationTurnStarted() override;

  // Invoked when the current conversation turn has finished.
  void OnInteractionFinished(Resolution resolution);

  AudioInputProviderImpl& audio_input_provider() {
    return audio_input_provider_;
  }

 private:
  AudioInputImpl& audio_input();

  mojo::Receiver<mojom::AudioInputController> receiver_{this};
  AudioInputProviderImpl audio_input_provider_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_INPUT_CONTROLLER_H_
