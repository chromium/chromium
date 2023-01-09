// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/web_codecs/stream_control_channel.h"
#include "base/task/sequenced_task_runner.h"

#include <utility>

namespace cast_streaming::webcodecs {

StreamControlChannel::StreamControlChannel(
    Client* client,
    mojo::PendingAssociatedReceiver<mojom::DemuxerConnector> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : client_(client),
      task_runner_(std::move(task_runner)),
      receiver_(this, std::move(receiver), task_runner_) {
  DCHECK(client_);
  DCHECK(task_runner_);
}

StreamControlChannel::~StreamControlChannel() = default;

void StreamControlChannel::OnJavascriptConfigured() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_javascript_been_configured_ = true;

  if (enable_receiver_callback_) {
    std::move(enable_receiver_callback_).Run();
  }
}

void StreamControlChannel::OnNewBufferProvider(
    base::WeakPtr<AudioDecoderBufferProvider> ptr) {
  client_->OnNewAudioBufferProvider(std::move(ptr));
}

void StreamControlChannel::OnNewBufferProvider(
    base::WeakPtr<VideoDecoderBufferProvider> ptr) {
  client_->OnNewVideoBufferProvider(std::move(ptr));
}

void StreamControlChannel::EnableReceiver(EnableReceiverCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enable_receiver_callback_ = std::move(callback);

  if (has_javascript_been_configured_) {
    std::move(enable_receiver_callback_).Run();
  }
}

void StreamControlChannel::OnStreamsInitialized(
    mojom::AudioStreamInitializationInfoPtr audio_stream_info,
    mojom::VideoStreamInitializationInfoPtr video_stream_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (audio_stream_info) {
    mojom::AudioStreamInfoPtr& stream_init_info =
        audio_stream_info->stream_initialization_info;
    audio_buffer_requester_ = std::make_unique<AudioBufferRequester>(
        this, std::move(stream_init_info->decoder_config),
        std::move(stream_init_info->data_pipe),
        std::move(audio_stream_info->buffer_requester), task_runner_);
  }
  if (video_stream_info) {
    mojom::VideoStreamInfoPtr& stream_init_info =
        video_stream_info->stream_initialization_info;
    video_buffer_requester_ = std::make_unique<VideoBufferRequester>(
        this, std::move(stream_init_info->decoder_config),
        std::move(stream_init_info->data_pipe),
        std::move(video_stream_info->buffer_requester), task_runner_);
  }
}

}  // namespace cast_streaming::webcodecs
