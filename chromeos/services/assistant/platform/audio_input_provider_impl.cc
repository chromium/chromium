// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_provider_impl.h"

#include "chromeos/services/assistant/platform/audio_stream_factory_delegate.h"
#include "chromeos/services/assistant/public/cpp/features.h"

namespace chromeos {
namespace assistant {

AudioInputProviderImpl::AudioInputProviderImpl()
    : audio_stream_factory_delegate_(
          std::make_unique<DefaultAudioStreamFactoryDelegate>()),
      audio_input_(audio_stream_factory_delegate_.get(),
                   /*device_id=*/std::string()) {}

AudioInputProviderImpl::~AudioInputProviderImpl() = default;

AudioInputImpl& AudioInputProviderImpl::GetAudioInput() {
  return audio_input_;
}

int64_t AudioInputProviderImpl::GetCurrentAudioTime() {
  if (features::IsAudioEraserEnabled())
    return base::TimeTicks::Now().since_origin().InMicroseconds();

  return 0;
}

}  // namespace assistant
}  // namespace chromeos
