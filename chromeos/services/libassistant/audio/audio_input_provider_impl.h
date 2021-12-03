// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_INPUT_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_INPUT_PROVIDER_IMPL_H_

#include <cstdint>

#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/services/libassistant/audio/audio_input_impl.h"

namespace chromeos {
namespace libassistant {

class AudioInputProviderImpl : public assistant_client::AudioInputProvider {
 public:
  AudioInputProviderImpl();
  AudioInputProviderImpl(const AudioInputProviderImpl&) = delete;
  AudioInputProviderImpl& operator=(const AudioInputProviderImpl&) = delete;
  ~AudioInputProviderImpl() override;

  // assistant_client::AudioInputProvider overrides:
  AudioInputImpl& GetAudioInput() override;
  int64_t GetCurrentAudioTime() override;

 private:
  AudioInputImpl audio_input_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_INPUT_PROVIDER_IMPL_H_
