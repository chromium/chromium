// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_STREAM_HANDLER_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_STREAM_HANDLER_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "libassistant/shared/public/platform_audio_output.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace assistant {

class AudioMediaDataSource;

class AudioStreamHandler : public mojom::AssistantAudioDecoderClient,
                           public assistant_client::AudioOutput::Delegate {
 public:
  using InitCB =
      base::OnceCallback<void(const assistant_client::OutputStreamFormat&)>;

  explicit AudioStreamHandler(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~AudioStreamHandler() override;

  // Called on main thread.
  void StartAudioDecoder(
      mojom::AssistantAudioDecoderFactory* audio_decoder_factory,
      assistant_client::AudioOutput::Delegate* delegate,
      InitCB start_device_owner_on_main_thread);

  // mojom::AssistantAudioDecoderClient overrides:
  // Called by |audio_decoder_| on utility thread.
  void OnNewBuffers(const std::vector<std::vector<uint8_t>>& buffers) override;

  // assistant_client::AudioOutput::Delegate overrides:
  // Called by AudioDeviceOwner on main thread.
  void FillBuffer(void* buffer,
                  int buffer_size,
                  int64_t playback_timestamp,
                  assistant_client::Callback1<int> on_decoded) override;
  void OnEndOfStream() override;
  void OnError(assistant_client::AudioOutput::Error error) override;
  void OnStopped() override;

 private:
  // Calls AudioDeviceOwner to start on main thread.
  void OnDecoderInitialized(bool success,
                            uint32_t bytes_per_sample,
                            uint32_t samples_per_second,
                            uint32_t channels);
  void OnDecoderInitializedOnThread(bool success,
                                    uint32_t bytes_per_sample,
                                    uint32_t samples_per_second,
                                    uint32_t channels);
  void StopDelegate();

  // Called by |FillBuffer()| to fill available data. If no available data, it
  // will call |DecodeOnThread()| to get more data.
  void FillDecodedBuffer(void* buffer, int buffer_size);

  // Fills buffer to AudioDeviceOwner on main thread.
  void OnFillBufferOnThread(assistant_client::Callback1<int> on_decoded,
                            int num_bytes);

  // Calls |audio_decoder_| to decode on main thread.
  void DecodeOnThread();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  assistant_client::AudioOutput::Delegate* delegate_;

  mojo::Receiver<mojom::AssistantAudioDecoderClient> client_receiver_{this};
  std::unique_ptr<AudioMediaDataSource> media_data_source_;
  mojo::Remote<mojom::AssistantAudioDecoder> audio_decoder_;

  // True when there is more decoded data.
  bool no_more_data_ = false;
  // True if |Decode()| called and not all decoded buffers are received, e.g.
  // |buffers_to_receive_| != 0.
  bool is_decoding_ = false;
  // True after |OnStopped()| called.
  bool stopped_ = false;

  // Temporary storage of |buffer| passed by |FillBuffer|.
  void* buffer_to_copy_ = nullptr;
  // Temporary storage of |buffer_size| passed by |FillBuffer|.
  int size_to_copy_ = 0;
  // Temporary storage of |on_filled| passed by |FillBuffer|.
  assistant_client::Callback1<int> on_filled_;

  InitCB start_device_owner_on_main_thread_;

  base::circular_deque<std::vector<uint8_t>> decoded_data_;

  base::WeakPtrFactory<AudioStreamHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioStreamHandler);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_STREAM_HANDLER_H_
