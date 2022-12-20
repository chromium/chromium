// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_FACTORY_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_FACTORY_H_

#include "chromeos/ash/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::assistant {

class AssistantAudioDecoderFactory
    : public mojom::AssistantAudioDecoderFactory {
 public:
  explicit AssistantAudioDecoderFactory(
      mojo::PendingReceiver<mojom::AssistantAudioDecoderFactory> receiver);

  AssistantAudioDecoderFactory(const AssistantAudioDecoderFactory&) = delete;
  AssistantAudioDecoderFactory& operator=(const AssistantAudioDecoderFactory&) =
      delete;

  ~AssistantAudioDecoderFactory() override;

 private:
  // mojom::AssistantAudioDecoderFactory:
  void CreateAssistantAudioDecoder(
      mojo::PendingReceiver<mojom::AssistantAudioDecoder> receiver,
      mojo::PendingRemote<mojom::AssistantAudioDecoderClient> client,
      mojo::PendingRemote<mojom::AssistantMediaDataSource> data_source)
      override;

  mojo::Receiver<mojom::AssistantAudioDecoderFactory> receiver_;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_FACTORY_H_
