// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/receiver_session_impl.h"

#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/browser/cast_message_port_converter.h"
#include "components/cast_streaming/browser/public/network_context_getter.h"
#include "components/cast_streaming/browser/public/receiver_config.h"
#include "components/cast_streaming/common/public/features.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver_constraints.h"

namespace cast_streaming {

// static
std::unique_ptr<ReceiverSession> ReceiverSession::Create(
    ReceiverConfig av_constraints,
    ReceiverSession::MessagePortProvider message_port_provider,
    ReceiverSession::Client* client) {
  return std::make_unique<ReceiverSessionImpl>(
      std::move(av_constraints), std::move(message_port_provider), client);
}

ReceiverSessionImpl::ReceiverSessionImpl(
    ReceiverConfig av_constraints,
    ReceiverSession::MessagePortProvider message_port_provider,
    ReceiverSession::Client* client)
    : message_port_provider_(std::move(message_port_provider)),
      av_constraints_(std::move(av_constraints)),
      client_(client),
      weak_factory_(this) {
  // TODO(crbug.com/40771653): Validate the provided codecs against build flags.
  DCHECK(message_port_provider_);
}

ReceiverSessionImpl::~ReceiverSessionImpl() = default;

void ReceiverSessionImpl::StartStreamingAsync(
    mojo::AssociatedRemote<mojom::DemuxerConnector> demuxer_connector) {
  DCHECK(!IsCastRemotingEnabled());
  StartStreamingAsyncInternal(std::move(demuxer_connector));
}

void ReceiverSessionImpl::StartStreamingAsync(
    mojo::AssociatedRemote<mojom::DemuxerConnector> demuxer_connector,
    mojo::AssociatedRemote<mojom::RendererController> renderer_controller) {
  DCHECK(IsCastRemotingEnabled());
  DCHECK(!renderer_control_config_);
  external_renderer_controls_ =
      std::make_unique<RendererControllerImpl>(base::BindOnce(
          &ReceiverSessionImpl::OnMojoDisconnect, weak_factory_.GetWeakPtr()));
  renderer_control_config_.emplace(std::move(renderer_controller),
                                   external_renderer_controls_->Bind());

  StartStreamingAsyncInternal(std::move(demuxer_connector));
}

void ReceiverSessionImpl::StartStreamingAsyncInternal(
    mojo::AssociatedRemote<mojom::DemuxerConnector> demuxer_connector) {
  DCHECK(HasNetworkContextGetter());

  DVLOG(1) << __func__;
  demuxer_connector_ = std::move(demuxer_connector);

  demuxer_connector_->EnableReceiver(base::BindOnce(
      &ReceiverSessionImpl::OnReceiverEnabled, weak_factory_.GetWeakPtr()));
  demuxer_connector_.set_disconnect_handler(base::BindOnce(
      &ReceiverSessionImpl::OnMojoDisconnect, weak_factory_.GetWeakPtr()));
}

ReceiverSession::RendererController*
ReceiverSessionImpl::GetRendererControls() {
  DCHECK(external_renderer_controls_);
  return external_renderer_controls_.get();
}

void ReceiverSessionImpl::OnReceiverEnabled() {
  DVLOG(1) << __func__;
  cast_streaming_session_.Start(this, std::move(renderer_control_config_),
                                std::move(av_constraints_),
                                std::move(message_port_provider_),
                                base::SequencedTaskRunner::GetCurrentDefault());
}

void ReceiverSessionImpl::OnSessionInitialization(
    StreamingInitializationInfo initialization_info,
    std::optional<mojo::ScopedDataPipeConsumerHandle> audio_pipe_consumer,
    std::optional<mojo::ScopedDataPipeConsumerHandle> video_pipe_consumer) {
  DVLOG(1) << __func__;
  DCHECK_EQ(!!initialization_info.audio_stream_info, !!audio_pipe_consumer);
  DCHECK_EQ(!!initialization_info.video_stream_info, !!video_pipe_consumer);
  DCHECK(audio_pipe_consumer || video_pipe_consumer);

  mojom::AudioStreamInitializationInfoPtr audio_info;
  if (audio_pipe_consumer) {
    mojo::PendingRemote<mojom::AudioBufferRequester> audio_receiver;
    audio_demuxer_stream_data_provider_ =
        std::make_unique<AudioDemuxerStreamDataProvider>(
            audio_receiver.InitWithNewPipeAndPassReceiver(),
            cast_streaming_session_.GetAudioBufferRequester(),
            base::BindOnce(&ReceiverSessionImpl::OnMojoDisconnect,
                           weak_factory_.GetWeakPtr()),
            std::move(initialization_info.audio_stream_info->config));
    audio_demuxer_stream_data_provider_->SetClient(std::move(
        initialization_info.audio_stream_info->demuxer_stream_client));
    audio_info = mojom::AudioStreamInitializationInfo::New(
        std::move(audio_receiver),
        mojom::AudioStreamInfo::New(
            audio_demuxer_stream_data_provider_->config(),
            std::move(std::move(audio_pipe_consumer.value()))));
  }

  mojom::VideoStreamInitializationInfoPtr video_info;
  if (video_pipe_consumer) {
    mojo::PendingRemote<mojom::VideoBufferRequester> video_receiver;
    video_demuxer_stream_data_provider_ =
        std::make_unique<VideoDemuxerStreamDataProvider>(
            video_receiver.InitWithNewPipeAndPassReceiver(),
            cast_streaming_session_.GetVideoBufferRequester(),
            base::BindOnce(&ReceiverSessionImpl::OnMojoDisconnect,
                           weak_factory_.GetWeakPtr()),
            std::move(initialization_info.video_stream_info->config));
    video_demuxer_stream_data_provider_->SetClient(std::move(
        initialization_info.video_stream_info->demuxer_stream_client));
    video_info = mojom::VideoStreamInitializationInfo::New(
        std::move(video_receiver),
        mojom::VideoStreamInfo::New(
            video_demuxer_stream_data_provider_->config(),
            std::move(std::move(video_pipe_consumer.value()))));
  }

  demuxer_connector_->OnStreamsInitialized(std::move(audio_info),
                                           std::move(video_info));

  PreloadBuffersAndStartPlayback();
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

void ReceiverSessionImpl::OnSessionReinitializationPending() {
  if (audio_demuxer_stream_data_provider_) {
    audio_demuxer_stream_data_provider_->WaitForNewStreamInfo();
  }
  if (video_demuxer_stream_data_provider_) {
    video_demuxer_stream_data_provider_->WaitForNewStreamInfo();
  }
}

void ReceiverSessionImpl::OnSessionReinitialization(
    StreamingInitializationInfo initialization_info,
    std::optional<mojo::ScopedDataPipeConsumerHandle> audio_pipe_consumer,
    std::optional<mojo::ScopedDataPipeConsumerHandle> video_pipe_consumer) {
  DVLOG(1) << __func__;
  DCHECK(audio_pipe_consumer || video_pipe_consumer);
  DCHECK_EQ(!!audio_pipe_consumer, !!initialization_info.audio_stream_info);
  DCHECK_EQ(!!video_pipe_consumer, !!initialization_info.video_stream_info);
  DCHECK_EQ(!!audio_pipe_consumer, !!audio_demuxer_stream_data_provider_);
  DCHECK_EQ(!!video_pipe_consumer, !!video_demuxer_stream_data_provider_);

  if (audio_pipe_consumer) {
    if (!audio_demuxer_stream_data_provider_->config().Matches(
            initialization_info.audio_stream_info->config)) {
      audio_demuxer_stream_data_provider_->SetClient(std::move(
          initialization_info.audio_stream_info->demuxer_stream_client));
      audio_demuxer_stream_data_provider_->OnNewStreamInfo(
          std::move(initialization_info.audio_stream_info->config),
          std::move(*audio_pipe_consumer));
    } else {
      DVLOG(1) << "Skipping application of new AudioDecoderConfig as no "
                  "config parameters have changed";
    }
  }

  if (video_pipe_consumer) {
    if (!video_demuxer_stream_data_provider_->config().Matches(
            initialization_info.video_stream_info->config)) {
      video_demuxer_stream_data_provider_->SetClient(std::move(
          initialization_info.video_stream_info->demuxer_stream_client));
      video_demuxer_stream_data_provider_->OnNewStreamInfo(
          std::move(initialization_info.video_stream_info->config),
          std::move(*video_pipe_consumer));
    } else {
      DVLOG(1) << "Skipping application of new VideoDecoderConfig as no "
                  "config parameters have changed";
    }
  }

  PreloadBuffersAndStartPlayback();
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
  demuxer_connector_.reset();

  // Tear down all remaining Mojo objects if needed. This is necessary if the
  // Cast Streaming Session ending was initiated by the receiver component.
  audio_demuxer_stream_data_provider_.reset();
  video_demuxer_stream_data_provider_.reset();

  if (client_) {
    client_->OnStreamingSessionEnded();
  }
}

void ReceiverSessionImpl::PreloadBuffersAndStartPlayback() {
  DCHECK(audio_demuxer_stream_data_provider_ ||
         video_demuxer_stream_data_provider_);
  DVLOG(1) << __func__;

  if (audio_demuxer_stream_data_provider_) {
    audio_demuxer_stream_data_provider_->PreloadBuffer(
        cast_streaming_session_.GetAudioBufferPreloader());
  }

  if (video_demuxer_stream_data_provider_) {
    video_demuxer_stream_data_provider_->PreloadBuffer(
        cast_streaming_session_.GetVideoBufferPreloader());
  }
}

void ReceiverSessionImpl::OnMojoDisconnect() {
  DVLOG(1) << __func__;

  // Close the underlying connection. This should only occur if a mojo
  // disconnection occurs very early in the initialization of this component -
  // specifically, before the browser and renderer processes have successfully
  // connected via mojom::DemuxerConnector::EnableReceiver().
  if (message_port_provider_) {
    // Create this and immediately delete it to create the associated message
    // port and delete it without including the MessagePort header.
    CastMessagePortConverter::Create(std::move(message_port_provider_),
                                     base::OnceClosure());
  }

  // Close the Cast Streaming Session. OnSessionEnded() will be called as part
  // of the Session shutdown, which will tear down the Mojo connection.
  if (cast_streaming_session_.is_running()) {
    cast_streaming_session_.Stop();
  }

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
