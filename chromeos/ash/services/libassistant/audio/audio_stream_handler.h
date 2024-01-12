// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_AUDIO_STREAM_HANDLER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_AUDIO_STREAM_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::libassistant {

class AudioMediaDataSource;

class AudioStreamHandler : public assistant::mojom::AssistantAudioDecoderClient,
                           public assistant_client::AudioOutput::Delegate {
 public:
  using InitCB =
      base::OnceCallback<void(const assistant_client::OutputStreamFormat&)>;

  AudioStreamHandler();

  AudioStreamHandler(const AudioStreamHandler&) = delete;
  AudioStreamHandler& operator=(const AudioStreamHandler&) = delete;

  ~AudioStreamHandler() override;

  void StartAudioDecoder(
      assistant::mojom::AssistantAudioDecoderFactory* audio_decoder_factory,
      assistant_client::AudioOutput::Delegate* delegate,
      InitCB start_device_owner_on_main_thread);

  // assistant::mojom::AssistantAudioDecoderClient overrides:
  void OnNewBuffers(const std::vector<std::vector<uint8_t>>& buffers) override;

  // assistant_client::AudioOutput::Delegate overrides:
  void FillBuffer(void* buffer,
                  int buffer_size,
                  int64_t playback_timestamp,
                  assistant_client::Callback1<int> on_decoded) override;
  void OnEndOfStream() override;
  void OnError(assistant_client::AudioOutput::Error error) override;
  void OnStopped() override;

 private:
  void OnDecoderInitialized(bool success,
                            uint32_t bytes_per_sample,
                            uint32_t samples_per_second,
                            uint32_t channels);
  void StopDelegate();

  void FillBufferOnMainThread(void* buffer,
                              int buffer_size,
                              assistant_client::Callback1<int> on_filled);

  // Called by |FillBufferOnMainThread()| to fill available data. If no
  // available data, it will call |Decode()| to get more data.
  void FillDecodedBuffer(void* buffer, int buffer_size);

  void OnFillBuffer(assistant_client::Callback1<int> on_decoded, int num_bytes);

  void Decode();

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  raw_ptr<assistant_client::AudioOutput::Delegate> delegate_;

  mojo::Receiver<AssistantAudioDecoderClient> client_receiver_{this};
  std::unique_ptr<AudioMediaDataSource> media_data_source_;
  mojo::Remote<assistant::mojom::AssistantAudioDecoder> audio_decoder_;

  // True when there is more decoded data.
  bool no_more_data_ = false;
  // True if |Decode()| called and not all decoded buffers are received, e.g.
  // |buffers_to_receive_| != 0.
  bool is_decoding_ = false;
  // True after |OnStopped()| called.
  bool stopped_ = false;

  // Temporary storage of |buffer| passed by |FillBuffer|.
  raw_ptr<void> buffer_to_copy_ = nullptr;
  // Temporary storage of |buffer_size| passed by |FillBuffer|.
  int size_to_copy_ = 0;
  // Temporary storage of |on_filled| passed by |FillBuffer|.
  assistant_client::Callback1<int> on_filled_;

  InitCB start_device_owner_on_main_thread_;

  base::circular_deque<std::vector<uint8_t>> decoded_data_;

  // The callbacks from Libassistant are called on a different sequence,
  // so this sequence checker ensures that no other methods are called on the
  // libassistant sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AudioStreamHandler> weak_factory_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_AUDIO_STREAM_HANDLER_H_
