// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/test/cast_streaming_test_receiver.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/cast_streaming/public/config_conversions.h"

namespace cast_streaming {

CastStreamingTestReceiver::CastStreamingTestReceiver() = default;
CastStreamingTestReceiver::~CastStreamingTestReceiver() = default;

void CastStreamingTestReceiver::Start(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port) {
  VLOG(1) << __func__;
  auto stream_config =
      std::make_unique<cast_streaming::ReceiverSession::AVConstraints>(
          ToVideoCaptureConfigCodecs(media::VideoCodec::kH264,
                                     media::VideoCodec::kVP8),
          ToAudioCaptureConfigCodecs(media::AudioCodec::kAAC,
                                     media::AudioCodec::kOpus));
  receiver_session_.Start(this, absl::nullopt, std::move(stream_config),
                          std::move(message_port),
                          base::SequencedTaskRunnerHandle::Get());
}

void CastStreamingTestReceiver::Stop() {
  VLOG(1) << __func__;
  is_active_ = false;
  receiver_session_.Stop();

  // Clear the configuration.
  audio_config_.reset();
  audio_decoder_buffer_reader_.reset();
  audio_buffers_.clear();
  video_config_.reset();
  video_decoder_buffer_reader_.reset();
  video_buffers_.clear();

  // Clear any pending callback.
  if (receiver_updated_closure_) {
    std::move(receiver_updated_closure_).Run();
  }
}

void CastStreamingTestReceiver::RunUntilStarted() {
  VLOG(1) << __func__;
  CHECK(!receiver_updated_closure_);
  while (!is_active_) {
    base::RunLoop run_loop;
    receiver_updated_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

void CastStreamingTestReceiver::RunUntilStopped() {
  VLOG(1) << __func__;
  CHECK(!receiver_updated_closure_);
  while (is_active_) {
    base::RunLoop run_loop;
    receiver_updated_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

bool CastStreamingTestReceiver::RunUntilAudioFramesCountIsAtLeast(
    size_t audio_frames_count) {
  VLOG(1) << __func__;
  CHECK(!receiver_updated_closure_);
  while (is_active_ && audio_buffers_.size() < audio_frames_count) {
    base::RunLoop run_loop;
    receiver_updated_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  return is_active_;
}

bool CastStreamingTestReceiver::RunUntilVideoFramesCountIsAtLeast(
    size_t video_frames_count) {
  VLOG(1) << __func__;
  CHECK(!receiver_updated_closure_);
  while (is_active_ && video_buffers_.size() < video_frames_count) {
    base::RunLoop run_loop;
    receiver_updated_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  return is_active_;
}

void CastStreamingTestReceiver::OnAudioBufferRead(
    scoped_refptr<media::DecoderBuffer> buffer) {
  VLOG(3) << __func__;

  // The pending buffer reads are cancelled when we reset the data pipe on a
  // configuration change. Just ignore them and return early here.
  if (!buffer)
    return;

  audio_buffers_.push_back(buffer);
  if (receiver_updated_closure_) {
    std::move(receiver_updated_closure_).Run();
  }
}

void CastStreamingTestReceiver::OnVideoBufferRead(
    scoped_refptr<media::DecoderBuffer> buffer) {
  VLOG(3) << __func__;

  // The pending buffer reads are cancelled when we reset the data pipe on a
  // configuration change. Just ignore them and return early here.
  if (!buffer)
    return;

  video_buffers_.push_back(buffer);
  if (receiver_updated_closure_) {
    std::move(receiver_updated_closure_).Run();
  }
}

void CastStreamingTestReceiver::OnSessionInitialization(
    StreamingInitializationInfo initialization_info,
    absl::optional<mojo::ScopedDataPipeConsumerHandle> audio_pipe_consumer,
    absl::optional<mojo::ScopedDataPipeConsumerHandle> video_pipe_consumer) {
  VLOG(1) << __func__;
  if (initialization_info.audio_stream_info) {
    audio_decoder_buffer_reader_ =
        std::make_unique<media::MojoDecoderBufferReader>(
            std::move(audio_pipe_consumer.value()));
    audio_config_ = initialization_info.audio_stream_info->config;
  }

  if (initialization_info.video_stream_info) {
    video_decoder_buffer_reader_ =
        std::make_unique<media::MojoDecoderBufferReader>(
            std::move(video_pipe_consumer.value()));
    video_config_ = initialization_info.video_stream_info->config;
  }

  is_active_ = true;
  if (receiver_updated_closure_) {
    std::move(receiver_updated_closure_).Run();
  }
}

void CastStreamingTestReceiver::OnAudioBufferReceived(
    media::mojom::DecoderBufferPtr buffer) {
  VLOG(3) << __func__;
  audio_decoder_buffer_reader_->ReadDecoderBuffer(
      std::move(buffer),
      base::BindOnce(&CastStreamingTestReceiver::OnAudioBufferRead,
                     base::Unretained(this)));
}

void CastStreamingTestReceiver::OnVideoBufferReceived(
    media::mojom::DecoderBufferPtr buffer) {
  VLOG(3) << __func__;
  video_decoder_buffer_reader_->ReadDecoderBuffer(
      std::move(buffer),
      base::BindOnce(&CastStreamingTestReceiver::OnVideoBufferRead,
                     base::Unretained(this)));
}

void CastStreamingTestReceiver::OnSessionReinitialization(
    StreamingInitializationInfo initialization_info,
    absl::optional<mojo::ScopedDataPipeConsumerHandle> audio_pipe_consumer,
    absl::optional<mojo::ScopedDataPipeConsumerHandle> video_pipe_consumer) {
  VLOG(1) << __func__;

  // TODO(crbug.com/1110490): Add tests handling the session reinitialization
  // case.
  Stop();
}

void CastStreamingTestReceiver::OnSessionEnded() {
  VLOG(1) << __func__;
  if (is_active_) {
    // Do not call Stop() if the session is already ending.
    Stop();
  }
}

}  // namespace cast_streaming
