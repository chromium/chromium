// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/proxy/audio_input_bindings.h"

#include <utility>

namespace chromeos {
namespace assistant {

AudioInputBindings::AudioInputBindings(
    mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
        pending_audio_input_controller_remote,
    mojo::PendingReceiver<
        chromeos::libassistant::mojom::AudioStreamFactoryDelegate>
        pending_audio_stream_factory_delegate_receiver)
    : pending_audio_input_controller_remote(
          std::move(pending_audio_input_controller_remote)),
      pending_audio_stream_factory_delegate_receiver(
          std::move(pending_audio_stream_factory_delegate_receiver)) {}

AudioInputBindings::AudioInputBindings(AudioInputBindings&&) = default;
AudioInputBindings& AudioInputBindings::operator=(AudioInputBindings&&) =
    default;
AudioInputBindings::~AudioInputBindings() = default;

}  // namespace assistant
}  // namespace chromeos
