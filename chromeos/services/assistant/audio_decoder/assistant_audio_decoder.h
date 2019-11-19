// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_H_
#define CHROMEOS_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {
class AudioFileReader;
class AudioBus;
class DataSource;
}  // namespace media

namespace chromeos {
namespace assistant {

class AssistantAudioDecoder : public mojom::AssistantAudioDecoder {
 public:
  AssistantAudioDecoder(
      mojo::PendingRemote<mojom::AssistantAudioDecoderClient> client,
      mojo::PendingRemote<mojom::AssistantMediaDataSource> data_source);
  ~AssistantAudioDecoder() override;

  // Called by |client_| on main thread.
  void OpenDecoder(OpenDecoderCallback callback) override;
  void Decode() override;
  void CloseDecoder(CloseDecoderCallback callback) override;

 private:
  // Calls |decoder_| to decode on media thread.
  void OpenDecoderOnMediaThread();
  void DecodeOnMediaThread();
  void CloseDecoderOnMediaThread();

  // Calls |client_| methods on main thread.
  void OnDecoderInitializedOnThread(int sample_rate, int channels);
  void OnBufferDecodedOnThread(
      const std::vector<std::unique_ptr<media::AudioBus>>&
          decoded_audio_buffers);

  void OnConnectionError();
  void RunCallbacksAsClosed();

  mojo::Remote<mojom::AssistantAudioDecoderClient> client_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  OpenDecoderCallback open_callback_;
  CloseDecoderCallback close_callback_;
  bool closed_ = false;

  std::unique_ptr<media::DataSource> data_source_;
  std::unique_ptr<media::BlockingUrlProtocol> protocol_;
  std::unique_ptr<media::AudioFileReader> decoder_;
  std::unique_ptr<base::Thread> media_thread_;

  base::WeakPtrFactory<AssistantAudioDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAudioDecoder);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_H_
