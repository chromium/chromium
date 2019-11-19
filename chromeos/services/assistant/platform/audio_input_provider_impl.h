// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_PROVIDER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/assistant/platform/audio_input_impl.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "libassistant/shared/public/platform_audio_input.h"

namespace chromeos {
class CrasAudioHandler;
class PowerManagerClient;

namespace assistant {

class AudioInputProviderImpl : public assistant_client::AudioInputProvider {
 public:
  AudioInputProviderImpl(mojom::Client* client,
                         PowerManagerClient* power_manager_client,
                         CrasAudioHandler* cras_audio_handler);
  ~AudioInputProviderImpl() override;

  // assistant_client::AudioInputProvider overrides:
  AudioInputImpl& GetAudioInput() override;
  int64_t GetCurrentAudioTime() override;

  // Called when the mic state associated with the interaction is changed.
  void SetMicState(bool mic_open);

  // Called when hotword enabled status changed.
  void OnHotwordEnabled(bool enable);

  // Setting the input device to use for audio capture.
  void SetDeviceId(const std::string& device_id);

  // Setting the hotword input device with hardware based hotword detection.
  void SetHotwordDeviceId(const std::string& device_id);

  // Setting the hotword locale for the input device with DSP support.
  void SetDspHotwordLocale(std::string pref_locale);

 private:
  AudioInputImpl audio_input_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputProviderImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_PROVIDER_IMPL_H_
