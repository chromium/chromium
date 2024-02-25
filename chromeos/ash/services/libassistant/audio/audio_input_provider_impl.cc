// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/audio/audio_input_provider_impl.h"

#include "base/time/time.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"

namespace ash::libassistant {

AudioInputProviderImpl::AudioInputProviderImpl()
    : audio_input_(/*device_id=*/std::nullopt) {}

AudioInputProviderImpl::~AudioInputProviderImpl() = default;

AudioInputImpl& AudioInputProviderImpl::GetAudioInput() {
  return audio_input_;
}

int64_t AudioInputProviderImpl::GetCurrentAudioTime() {
  if (assistant::features::IsAudioEraserEnabled())
    return base::TimeTicks::Now().since_origin().InMicroseconds();

  return 0;
}

}  // namespace ash::libassistant
