// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/receiver_session_impl.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/cast_streaming/browser/public/network_context_getter.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/mojom/media_types.mojom.h"

namespace cast_streaming {

// static
std::unique_ptr<ReceiverSession> ReceiverSession::Create(
    std::unique_ptr<ReceiverSession::AVConstraints> av_constraints,
    ReceiverSession::MessagePortProvider message_port_provider) {
  return std::make_unique<ReceiverSessionImpl>(
      std::move(av_constraints), std::move(message_port_provider));
}

ReceiverSessionImpl::ReceiverSessionImpl(
    std::unique_ptr<ReceiverSession::AVConstraints> av_constraints,
    ReceiverSession::MessagePortProvider message_port_provider)
    : message_port_provider_(std::move(message_port_provider)),
      av_constraints_(std::move(av_constraints)) {
  // TODO(crbug.com/1218495): Validate the provided codecs against build flags.
  DCHECK(av_constraints_);
  DCHECK(message_port_provider_);
}

ReceiverSessionImpl::~ReceiverSessionImpl() = default;

void ReceiverSessionImpl::SetCastStreamingReceiver(
    mojo::AssociatedRemote<mojom::CastStreamingReceiver>
        cast_streaming_receiver) {
  DCHECK(HasNetworkContextGetter());

  DVLOG(1) << __func__;
  cast_streaming_receiver_ = std::move(cast_streaming_receiver);

  // It is fine to use an unretained pointer to |this| here as the
  // AssociatedRemote, is owned by |this| and will be torn-down at the same time
  // as |this|.
  cast_streaming_receiver_->EnableReceiver(base::BindOnce(
      &ReceiverSessionImpl::OnReceiverEnabled, base::Unretained(this)));
  cast_streaming_receiver_.set_disconnect_handler(base::BindOnce(
      &ReceiverSessionImpl::OnMojoDisconnect, base::Unretained(this)));
}

void ReceiverSessionImpl::OnReceiverEnabled() {
  DVLOG(1) << __func__;
  DCHECK(message_port_provider_);
  cast_streaming_session_.Start(this, std::move(av_constraints_),
                                std::move(message_port_provider_).Run(),
                                base::SequencedTaskRunnerHandle::Get());
}

void ReceiverSessionImpl::OnSessionInitialization(
    absl::optional<cast_streaming::CastStreamingSession::AudioStreamInfo>
        audio_stream_info,
    absl::optional<cast_streaming::CastStreamingSession::VideoStreamInfo>
        video_stream_info) {
  DVLOG(1) << __func__;
  DCHECK(audio_stream_info || video_stream_info);

  mojom::AudioStreamInfoPtr mojo_audio_stream_info;
  if (audio_stream_info) {
    mojo_audio_stream_info =
        mojom::AudioStreamInfo::New(audio_stream_info->decoder_config,
                                    audio_remote_.BindNewPipeAndPassReceiver(),
                                    std::move(audio_stream_info->data_pipe));
  }

  mojom::VideoStreamInfoPtr mojo_video_stream_info;
  if (video_stream_info) {
    mojo_video_stream_info =
        mojom::VideoStreamInfo::New(video_stream_info->decoder_config,
                                    video_remote_.BindNewPipeAndPassReceiver(),
                                    std::move(video_stream_info->data_pipe));
  }

  cast_streaming_receiver_->OnStreamsInitialized(
      std::move(mojo_audio_stream_info), std::move(mojo_video_stream_info));
}

void ReceiverSessionImpl::OnAudioBufferReceived(
    media::mojom::DecoderBufferPtr buffer) {
  DVLOG(3) << __func__;
  DCHECK(audio_remote_);
  audio_remote_->ProvideBuffer(std::move(buffer));
}

void ReceiverSessionImpl::OnVideoBufferReceived(
    media::mojom::DecoderBufferPtr buffer) {
  DVLOG(3) << __func__;
  DCHECK(video_remote_);
  video_remote_->ProvideBuffer(std::move(buffer));
}

void ReceiverSessionImpl::OnSessionReinitialization(
    absl::optional<cast_streaming::CastStreamingSession::AudioStreamInfo>
        audio_stream_info,
    absl::optional<cast_streaming::CastStreamingSession::VideoStreamInfo>
        video_stream_info) {
  DVLOG(1) << __func__;
  DCHECK(audio_stream_info || video_stream_info);

  if (audio_stream_info) {
    audio_remote_->OnNewAudioConfig(audio_stream_info->decoder_config,
                                    std::move(audio_stream_info->data_pipe));
  }

  if (video_stream_info) {
    video_remote_->OnNewVideoConfig(video_stream_info->decoder_config,
                                    std::move(video_stream_info->data_pipe));
  }
}

void ReceiverSessionImpl::OnSessionEnded() {
  DVLOG(1) << __func__;

  // Tear down the Mojo connection.
  cast_streaming_receiver_.reset();

  // Tear down all remaining Mojo objects if needed. This is necessary if the
  // Cast Streaming Session ending was initiated by the receiver component.
  if (audio_remote_)
    audio_remote_.reset();
  if (video_remote_)
    video_remote_.reset();
}

void ReceiverSessionImpl::OnMojoDisconnect() {
  DVLOG(1) << __func__;

  // Close the underlying connection.
  if (message_port_provider_) {
    av_constraints_ = std::make_unique<ReceiverSession::AVConstraints>();
    std::move(message_port_provider_).Run().reset();
  }

  // Close the Cast Streaming Session. OnSessionEnded() will be called as part
  // of the Session shutdown, which will tear down the Mojo connection.
  cast_streaming_session_.Stop();

  // Tear down all remaining Mojo objects.
  audio_remote_.reset();
  video_remote_.reset();
}

}  // namespace cast_streaming
