// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/receiver_session_impl.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/cast_streaming/browser/public/network_context_getter.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"

namespace cast_streaming {

// static
std::unique_ptr<ReceiverSession> ReceiverSession::Create(
    std::unique_ptr<ReceiverSession::AVConstraints> av_constraints,
    ReceiverSession::MessagePortProvider message_port_provider,
    ReceiverSession::Client* client) {
  return std::make_unique<ReceiverSessionImpl>(
      std::move(av_constraints), std::move(message_port_provider), client);
}

ReceiverSessionImpl::ReceiverSessionImpl(
    std::unique_ptr<ReceiverSession::AVConstraints> av_constraints,
    ReceiverSession::MessagePortProvider message_port_provider,
    ReceiverSession::Client* client)
    : message_port_provider_(std::move(message_port_provider)),
      av_constraints_(std::move(av_constraints)),
      client_(client),
      weak_factory_(this) {
  // TODO(crbug.com/1218495): Validate the provided codecs against build flags.
  DCHECK(av_constraints_);
  DCHECK(message_port_provider_);
}

ReceiverSessionImpl::~ReceiverSessionImpl() = default;

void ReceiverSessionImpl::StartStreamingAsync(
    mojo::AssociatedRemote<mojom::CastStreamingReceiver>
        cast_streaming_receiver) {
  DCHECK(HasNetworkContextGetter());

  DVLOG(1) << __func__;
  cast_streaming_receiver_ = std::move(cast_streaming_receiver);

  cast_streaming_receiver_->EnableReceiver(base::BindOnce(
      &ReceiverSessionImpl::OnReceiverEnabled, weak_factory_.GetWeakPtr()));
  cast_streaming_receiver_.set_disconnect_handler(base::BindOnce(
      &ReceiverSessionImpl::OnMojoDisconnect, weak_factory_.GetWeakPtr()));
}

void ReceiverSessionImpl::StartStreamingAsync(
    mojo::AssociatedRemote<mojom::CastStreamingReceiver>
        cast_streaming_receiver,
    mojo::AssociatedRemote<mojom::RendererController> renderer_controller) {
  DCHECK(!renderer_control_config_);
  external_renderer_controls_ =
      std::make_unique<RendererControllerImpl>(base::BindOnce(
          &ReceiverSessionImpl::OnMojoDisconnect, weak_factory_.GetWeakPtr()));
  renderer_control_config_.emplace(std::move(renderer_controller),
                                   external_renderer_controls_->Bind());

  StartStreamingAsync(std::move(cast_streaming_receiver));
}

ReceiverSession::RendererController*
ReceiverSessionImpl::GetRendererControls() {
  DCHECK(external_renderer_controls_);
  return external_renderer_controls_.get();
}

void ReceiverSessionImpl::OnReceiverEnabled() {
  DVLOG(1) << __func__;
  DCHECK(message_port_provider_);
  cast_streaming_session_.Start(this, std::move(renderer_control_config_),
                                std::move(av_constraints_),
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

  mojom::AudioStreamInitializationInfoPtr audio_info;
  if (audio_stream_info) {
    mojo::PendingRemote<mojom::AudioBufferRequester> audio_receiver;
    audio_demuxer_stream_data_provider_ =
        std::make_unique<AudioDemuxerStreamDataProvider>(
            audio_receiver.InitWithNewPipeAndPassReceiver(),
            cast_streaming_session_.GetAudioBufferRequester(),
            base::BindOnce(&ReceiverSessionImpl::OnMojoDisconnect,
                           weak_factory_.GetWeakPtr()),
            std::move(audio_stream_info->decoder_config));
    audio_info = mojom::AudioStreamInitializationInfo::New(
        std::move(audio_receiver),
        mojom::AudioStreamInfo::New(
            audio_demuxer_stream_data_provider_->config(),
            std::move(std::move(audio_stream_info->data_pipe))));
  }

  mojom::VideoStreamInitializationInfoPtr video_info;
  if (video_stream_info) {
    mojo::PendingRemote<mojom::VideoBufferRequester> video_receiver;
    video_demuxer_stream_data_provider_ =
        std::make_unique<VideoDemuxerStreamDataProvider>(
            video_receiver.InitWithNewPipeAndPassReceiver(),
            cast_streaming_session_.GetVideoBufferRequester(),
            base::BindOnce(&ReceiverSessionImpl::OnMojoDisconnect,
                           weak_factory_.GetWeakPtr()),
            std::move(video_stream_info->decoder_config));
    video_info = mojom::VideoStreamInitializationInfo::New(
        std::move(video_receiver),
        mojom::VideoStreamInfo::New(
            video_demuxer_stream_data_provider_->config(),
            std::move(std::move(video_stream_info->data_pipe))));
  }

  cast_streaming_receiver_->OnStreamsInitialized(std::move(audio_info),
                                                 std::move(video_info));

  InformClientOfConfigChange();
}

void ReceiverSessionImpl::OnAudioBufferReceived(
    media::mojom::DecoderBufferPtr buffer) {
  DVLOG(3) << __func__;
  DCHECK(audio_demuxer_stream_data_provider_);
  audio_demuxer_stream_data_provider_->ProvideBuffer(std::move(buffer));
}

void ReceiverSessionImpl::OnVideoBufferReceived(
    media::mojom::DecoderBufferPtr buffer) {
  DVLOG(3) << __func__;
  DCHECK(video_demuxer_stream_data_provider_);
  video_demuxer_stream_data_provider_->ProvideBuffer(std::move(buffer));
}

void ReceiverSessionImpl::OnSessionReinitialization(
    absl::optional<cast_streaming::CastStreamingSession::AudioStreamInfo>
        audio_stream_info,
    absl::optional<cast_streaming::CastStreamingSession::VideoStreamInfo>
        video_stream_info) {
  DVLOG(1) << __func__;
  DCHECK(audio_stream_info || video_stream_info);
  DCHECK_EQ(!!audio_stream_info, !!audio_demuxer_stream_data_provider_);
  DCHECK_EQ(!!video_stream_info, !!video_demuxer_stream_data_provider_);

  if (audio_stream_info) {
    audio_demuxer_stream_data_provider_->OnNewStreamInfo(
        std::move(audio_stream_info->decoder_config),
        std::move(audio_stream_info->data_pipe));
  }

  if (video_stream_info) {
    video_demuxer_stream_data_provider_->OnNewStreamInfo(
        std::move(video_stream_info->decoder_config),
        std::move(video_stream_info->data_pipe));
  }

  InformClientOfConfigChange();
}

void ReceiverSessionImpl::InformClientOfConfigChange() {
  if (client_) {
    if (audio_demuxer_stream_data_provider_) {
      client_->OnAudioConfigUpdated(
          audio_demuxer_stream_data_provider_->config());
    }
    if (video_demuxer_stream_data_provider_) {
      client_->OnVideoConfigUpdated(
          video_demuxer_stream_data_provider_->config());
    }
  }
}

void ReceiverSessionImpl::OnSessionEnded() {
  DVLOG(1) << __func__;

  // Tear down the Mojo connection.
  cast_streaming_receiver_.reset();

  // Tear down all remaining Mojo objects if needed. This is necessary if the
  // Cast Streaming Session ending was initiated by the receiver component.
  audio_demuxer_stream_data_provider_.reset();
  video_demuxer_stream_data_provider_.reset();
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
  audio_demuxer_stream_data_provider_.reset();
  video_demuxer_stream_data_provider_.reset();
}

ReceiverSessionImpl::RendererControllerImpl::RendererControllerImpl(
    base::OnceCallback<void()> on_mojo_disconnect)
    : on_mojo_disconnect_(std::move(on_mojo_disconnect)) {
  DCHECK(on_mojo_disconnect_);
}

ReceiverSessionImpl::RendererControllerImpl::~RendererControllerImpl() =
    default;

bool ReceiverSessionImpl::RendererControllerImpl::IsValid() const {
  return renderer_controls_.is_bound() && renderer_controls_.is_connected();
}

void ReceiverSessionImpl::RendererControllerImpl::StartPlayingFrom(
    base::TimeDelta time) {
  DCHECK(IsValid());
  renderer_controls_->StartPlayingFrom(time);
}

void ReceiverSessionImpl::RendererControllerImpl::SetPlaybackRate(
    double playback_rate) {
  DCHECK(IsValid());
  renderer_controls_->SetPlaybackRate(playback_rate);
}

void ReceiverSessionImpl::RendererControllerImpl::SetVolume(float volume) {
  DCHECK(IsValid());
  renderer_controls_->SetVolume(volume);
}

mojo::PendingReceiver<media::mojom::Renderer>
ReceiverSessionImpl::RendererControllerImpl::Bind() {
  auto receiver = renderer_controls_.BindNewPipeAndPassReceiver();
  renderer_controls_.set_disconnect_handler(std::move(on_mojo_disconnect_));
  return receiver;
}

}  // namespace cast_streaming
