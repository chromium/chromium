// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_stream_handler.h"

#include "base/bind.h"
#include "chromeos/services/assistant/platform/audio_media_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {
namespace assistant {

AudioStreamHandler::AudioStreamHandler(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner), weak_factory_(this) {}

AudioStreamHandler::~AudioStreamHandler() = default;

void AudioStreamHandler::StartAudioDecoder(
    mojom::AssistantAudioDecoderFactory* audio_decoder_factory,
    assistant_client::AudioOutput::Delegate* delegate,
    InitCB on_inited) {
  mojo::PendingRemote<mojom::AssistantAudioDecoderClient> client;
  client_receiver_.Bind(client.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::AssistantMediaDataSource> data_source;
  media_data_source_ = std::make_unique<AudioMediaDataSource>(
      data_source.InitWithNewPipeAndPassReceiver(), task_runner_);

  audio_decoder_factory->CreateAssistantAudioDecoder(
      audio_decoder_.BindNewPipeAndPassReceiver(), std::move(client),
      std::move(data_source));

  delegate_ = delegate;
  media_data_source_->set_delegate(delegate_);
  start_device_owner_on_main_thread_ = std::move(on_inited);
  audio_decoder_->OpenDecoder(base::BindOnce(
      &AudioStreamHandler::OnDecoderInitialized, weak_factory_.GetWeakPtr()));
}

void AudioStreamHandler::OnNewBuffers(
    const std::vector<std::vector<uint8_t>>& buffers) {
  if (buffers.size() == 0)
    no_more_data_ = true;

  for (const auto& buffer : buffers)
    decoded_data_.emplace_back(buffer);

  is_decoding_ = false;
  FillDecodedBuffer(buffer_to_copy_, size_to_copy_);
}

// TODO(wutao): Needs to pass |playback_timestamp| to LibAssistant.
void AudioStreamHandler::FillBuffer(
    void* buffer,
    int buffer_size,
    int64_t playback_timestamp,
    assistant_client::Callback1<int> on_filled) {
  DCHECK(!on_filled_);

  on_filled_ = std::move(on_filled);
  buffer_to_copy_ = buffer;
  size_to_copy_ = buffer_size;

  FillDecodedBuffer(buffer, buffer_size);
}

void AudioStreamHandler::OnEndOfStream() {
  if (delegate_)
    delegate_->OnEndOfStream();
}

void AudioStreamHandler::OnError(assistant_client::AudioOutput::Error error) {
  if (delegate_)
    delegate_->OnError(error);
}

void AudioStreamHandler::OnStopped() {
  stopped_ = true;

  // Do not provide more source data.
  media_data_source_->set_delegate(nullptr);

  // Call |delegate_->OnStopped()| will delete |this|. Call |CloseDecoder| to
  // clean up first.
  audio_decoder_->CloseDecoder(base::BindOnce(&AudioStreamHandler::StopDelegate,
                                              weak_factory_.GetWeakPtr()));
}

void AudioStreamHandler::OnDecoderInitialized(bool success,
                                              uint32_t bytes_per_sample,
                                              uint32_t samples_per_second,
                                              uint32_t channels) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioStreamHandler::OnDecoderInitializedOnThread,
                     weak_factory_.GetWeakPtr(), success, bytes_per_sample,
                     samples_per_second, channels));
}

void AudioStreamHandler::OnDecoderInitializedOnThread(
    bool success,
    uint32_t bytes_per_sample,
    uint32_t samples_per_second,
    uint32_t channels) {
  if (!success) {
    // In the case that both |OpenDecoder()| and |CloseDecoder()| were called,
    // there is no need to call |OnError()|, since we are going to call
    // |OnStopped()| soon.
    if (!stopped_)
      OnError(assistant_client::AudioOutput::Error::FATAL_ERROR);

    start_device_owner_on_main_thread_.Reset();
    return;
  }

  DCHECK(bytes_per_sample == 2 || bytes_per_sample == 4);
  const assistant_client::OutputStreamFormat format = {
      bytes_per_sample == 2
          ? assistant_client::OutputStreamEncoding::STREAM_PCM_S16
          : assistant_client::OutputStreamEncoding::STREAM_PCM_S32,
      /*pcm_sample_rate=*/samples_per_second,
      /*pcm_num_channels=*/channels};
  if (start_device_owner_on_main_thread_) {
    DCHECK(!on_filled_);
    std::move(start_device_owner_on_main_thread_).Run(format);
  }
}

void AudioStreamHandler::StopDelegate() {
  delegate_->OnStopped();
  delegate_ = nullptr;
}

void AudioStreamHandler::FillDecodedBuffer(void* buffer, int buffer_size) {
  if (on_filled_ && (decoded_data_.size() > 0 || no_more_data_)) {
    int size_copied = 0;
    // Fill buffer with data not more than requested.
    while (!decoded_data_.empty() && size_copied < buffer_size) {
      std::vector<uint8_t>& data = decoded_data_.front();
      int audio_buffer_size = static_cast<int>(data.size());
      if (size_copied + audio_buffer_size > buffer_size)
        audio_buffer_size = buffer_size - size_copied;

      memcpy(reinterpret_cast<uint8_t*>(buffer) + size_copied, data.data(),
             audio_buffer_size);
      size_copied += audio_buffer_size;
      if (audio_buffer_size < static_cast<int>(data.size()))
        data.erase(data.begin(), data.begin() + audio_buffer_size);
      else
        decoded_data_.pop_front();
    }

    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AudioStreamHandler::OnFillBufferOnThread,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(on_filled_), size_copied));
  }

  if (decoded_data_.empty() && !no_more_data_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioStreamHandler::DecodeOnThread,
                                          weak_factory_.GetWeakPtr()));
  }
}

void AudioStreamHandler::OnFillBufferOnThread(
    assistant_client::Callback1<int> on_filled,
    int num_bytes) {
  on_filled(num_bytes);
}

void AudioStreamHandler::DecodeOnThread() {
  if (is_decoding_)
    return;

  is_decoding_ = true;
  audio_decoder_->Decode();
}

}  // namespace assistant
}  // namespace chromeos
