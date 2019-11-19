// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/audio_decoder/assistant_audio_decoder.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/threading/thread.h"
#include "chromeos/services/assistant/audio_decoder/ipc_data_source.h"
#include "media/base/audio_bus.h"
#include "media/base/data_source.h"
#include "media/filters/audio_file_reader.h"
#include "media/filters/blocking_url_protocol.h"

namespace chromeos {
namespace assistant {

namespace {

// Preferred bytes per sample when get interleaved data from AudioBus.
constexpr int kBytesPerSample = 2;

void OnError(bool* succeeded) {
  *succeeded = false;
}

}  // namespace

AssistantAudioDecoder::AssistantAudioDecoder(
    mojo::PendingRemote<mojom::AssistantAudioDecoderClient> client,
    mojo::PendingRemote<mojom::AssistantMediaDataSource> data_source)
    : client_(std::move(client)),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      data_source_(std::make_unique<IPCDataSource>(std::move(data_source))),
      media_thread_(std::make_unique<base::Thread>("media_thread")),
      weak_factory_(this) {
  CHECK(media_thread_->Start());
  client_.set_disconnect_handler(base::BindOnce(
      &AssistantAudioDecoder::OnConnectionError, base::Unretained(this)));
}

AssistantAudioDecoder::~AssistantAudioDecoder() = default;

void AssistantAudioDecoder::Decode() {
  media_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&AssistantAudioDecoder::DecodeOnMediaThread,
                                base::Unretained(this)));
}

void AssistantAudioDecoder::OpenDecoder(OpenDecoderCallback callback) {
  DCHECK(!open_callback_);
  open_callback_ = std::move(callback);
  media_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantAudioDecoder::OpenDecoderOnMediaThread,
                     base::Unretained(this)));
}

void AssistantAudioDecoder::CloseDecoder(CloseDecoderCallback callback) {
  DCHECK(!close_callback_);
  close_callback_ = std::move(callback);
  media_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantAudioDecoder::CloseDecoderOnMediaThread,
                     base::Unretained(this)));
}

void AssistantAudioDecoder::OpenDecoderOnMediaThread() {
  bool read_ok = true;
  protocol_ = std::make_unique<media::BlockingUrlProtocol>(
      data_source_.get(), base::BindRepeating(&OnError, &read_ok));
  decoder_ = std::make_unique<media::AudioFileReader>(protocol_.get());

  if (closed_ || !decoder_->Open() || !read_ok) {
    CloseDecoderOnMediaThread();
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantAudioDecoder::OnDecoderInitializedOnThread,
                     weak_factory_.GetWeakPtr(), decoder_->sample_rate(),
                     decoder_->channels()));
}

void AssistantAudioDecoder::DecodeOnMediaThread() {
  std::vector<std::unique_ptr<media::AudioBus>> decoded_audio_packets;
  // Experimental number of decoded packets before sending to |client_|.
  constexpr int kPacketsToRead = 16;
  DCHECK(decoder_);
  // The client expects to be called |OnNewBuffers()| so that to return
  // AudioDeviceOwner's |FillBuffer()| call. If |closed_| is true, still return
  // empty |decoded_audio_packets| to indicate no more data available.
  if (!closed_)
    decoder_->Read(&decoded_audio_packets, kPacketsToRead);

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AssistantAudioDecoder::OnBufferDecodedOnThread,
                                weak_factory_.GetWeakPtr(),
                                std::move(decoded_audio_packets)));
}

void AssistantAudioDecoder::CloseDecoderOnMediaThread() {
  // |decoder_| may not be initialized.
  if (decoder_)
    decoder_->Close();

  closed_ = true;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AssistantAudioDecoder::RunCallbacksAsClosed,
                                weak_factory_.GetWeakPtr()));
}

void AssistantAudioDecoder::OnDecoderInitializedOnThread(
    int sample_rate,
    int channels) {
  DCHECK(open_callback_);
  std::move(open_callback_)
      .Run(/*success=*/true, kBytesPerSample, sample_rate, channels);
}

void AssistantAudioDecoder::OnBufferDecodedOnThread(
    const std::vector<std::unique_ptr<media::AudioBus>>&
        decoded_audio_packets) {
  if (!client_)
    return;

  std::vector<std::vector<uint8_t>> buffers;
  for (const auto& audio_bus : decoded_audio_packets) {
    const int bytes_to_alloc =
        audio_bus->frames() * kBytesPerSample * audio_bus->channels();
    std::vector<uint8_t> buffer(bytes_to_alloc);
    audio_bus->ToInterleaved<media::SignedInt16SampleTypeTraits>(
        audio_bus->frames(), reinterpret_cast<int16_t*>(buffer.data()));
    buffers.emplace_back(buffer);
  }
  client_->OnNewBuffers(buffers);
}

void AssistantAudioDecoder::OnConnectionError() {
  client_.reset();
  media_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantAudioDecoder::CloseDecoderOnMediaThread,
                     base::Unretained(this)));
}

void AssistantAudioDecoder::RunCallbacksAsClosed() {
  if (open_callback_) {
    std::move(open_callback_)
        .Run(/*success=*/false,
             /*bytes_per_sample=*/0,
             /*samples_per_second=*/0,
             /*channels=*/0);
  }

  if (close_callback_)
    std::move(close_callback_).Run();
}

}  // namespace assistant
}  // namespace chromeos
