// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_HOST_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_HOST_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/services/assistant/platform/audio_devices.h"

namespace chromeos {
namespace assistant {

class AudioInputImpl;

// Class that provides the bridge between the ChromeOS UI thread and the
// Libassistant audio input class.
// The goal is that |AudioInputImpl| no longer depends on any external events.
// This will allow us to move it to the Libassistant mojom service (at which
// point this class will talk to the Libassistant mojom service).
class COMPONENT_EXPORT(ASSISTANT_SERVICE) AudioInputHost
    : private chromeos::PowerManagerClient::Observer,
      private AudioDevices::Observer

{
 public:
  AudioInputHost(AudioInputImpl* audio_input,
                 CrasAudioHandler* cras_audio_handler,
                 chromeos::PowerManagerClient* power_manager_client,
                 const std::string& locale);
  AudioInputHost(AudioInputHost&) = delete;
  AudioInputHost& operator=(AudioInputHost&) = delete;
  ~AudioInputHost() override;

  // Called when the mic state associated with the interaction is changed.
  void SetMicState(bool mic_open);

  // Called when hotword enabled status changed.
  void OnHotwordEnabled(bool enable);

  void OnConversationTurnStarted();
  void OnConversationTurnFinished();

  // AudioDevices::Observer implementation:
  void SetDeviceId(const base::Optional<std::string>& device_id) override;
  void SetHotwordDeviceId(
      const base::Optional<std::string>& device_id) override;

 private:
  // chromeos::PowerManagerClient::Observer overrides:
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;

  void OnInitialLidStateReceived(
      base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  // Owned by |PlatformApiImpl| which also owns |this|.
  AudioInputImpl* const audio_input_;
  chromeos::PowerManagerClient* const power_manager_client_;
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_;

  // Observes available audio devices and will set device-id/hotword-device-id
  // accordingly.
  AudioDevices audio_devices_;
  AudioDevices::ScopedObservation audio_devices_observation_{this};

  base::WeakPtrFactory<AudioInputHost> weak_factory_{this};
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_HOST_H_
