// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PROXY_AUDIO_INPUT_BINDINGS_H_
#define CHROMEOS_SERVICES_ASSISTANT_PROXY_AUDIO_INPUT_BINDINGS_H_

#include "chromeos/services/libassistant/public/mojom/audio_input_controller.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {
namespace assistant {

// Contains the bindings needed for the audio input handling.
struct AudioInputBindings {
  AudioInputBindings(
      mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
          pending_audio_input_controller_remote,
      mojo::PendingReceiver<
          chromeos::libassistant::mojom::AudioStreamFactoryDelegate>
          pending_audio_stream_factory_delegate_receiver);
  AudioInputBindings(AudioInputBindings&&);
  AudioInputBindings& operator=(AudioInputBindings&&);
  ~AudioInputBindings();

  mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
      pending_audio_input_controller_remote;
  mojo::PendingReceiver<
      chromeos::libassistant::mojom::AudioStreamFactoryDelegate>
      pending_audio_stream_factory_delegate_receiver;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PROXY_AUDIO_INPUT_BINDINGS_H_
