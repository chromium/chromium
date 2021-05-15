// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/audio/audio_input_provider_impl.h"

#include "base/time/time.h"
#include "chromeos/services/assistant/public/cpp/features.h"

namespace chromeos {
namespace libassistant {

AudioInputProviderImpl::AudioInputProviderImpl()
    : audio_input_(/*device_id=*/absl::nullopt) {}

AudioInputProviderImpl::~AudioInputProviderImpl() = default;

AudioInputImpl& AudioInputProviderImpl::GetAudioInput() {
  return audio_input_;
}

int64_t AudioInputProviderImpl::GetCurrentAudioTime() {
  if (chromeos::assistant::features::IsAudioEraserEnabled())
    return base::TimeTicks::Now().since_origin().InMicroseconds();

  return 0;
}

}  // namespace libassistant
}  // namespace chromeos
