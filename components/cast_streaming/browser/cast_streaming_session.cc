// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/cast_streaming_session.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/cast_streaming/browser/cast_message_port_converter.h"
#include "components/cast_streaming/browser/common/decoder_buffer_factory.h"
#include "components/cast_streaming/browser/control/remoting/remoting_decoder_buffer_factory.h"
#include "components/cast_streaming/browser/frame/mirroring_decoder_buffer_factory.h"
#include "components/cast_streaming/browser/frame/stream_consumer.h"
#include "components/cast_streaming/browser/receiver_config_conversions.h"
#include "components/cast_streaming/common/public/features.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/timestamp_constants.h"
#include "media/cast/openscreen/config_conversions.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace cast_streaming {
namespace {

// Timeout to stop the Session when no data is received.
constexpr base::TimeDelta kNoDataTimeout = base::Seconds(15);

bool CreateDataPipeForStreamType(media::DemuxerStream::Type type,
                                 mojo::ScopedDataPipeProducerHandle* producer,
                                 mojo::ScopedDataPipeConsumerHandle* consumer) {
  const MojoCreateDataPipeOptions data_pipe_options{
      sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE,
      1u /* element_num_bytes */,
      media::GetDefaultDecoderBufferConverterCapacity(type)};
  MojoResult result =
      mojo::CreateDataPipe(&data_pipe_options, *producer, *consumer);
  return result == MOJO_RESULT_OK;
}

// Timeout to end the Session when no offer message is sent.
constexpr base::TimeDelta kInitTimeout = base::Seconds(5);

StreamingInitializationInfo CreateMirroringInitializationInfo(
    const openscreen::cast::ReceiverSession* session,
    openscreen::cast::ReceiverSession::ConfiguredReceivers receivers) {
  std::optional<StreamingInitializationInfo::AudioStreamInfo> audio_stream_info;
  if (receivers.audio_receiver) {
    audio_stream_info.emplace(
        media::cast::ToAudioDecoderConfig(receivers.audio_config),
        receivers.audio_receiver);
  }

  std::optional<StreamingInitializationInfo::VideoStreamInfo> video_stream_info;
  if (receivers.video_receiver) {
    video_stream_info.emplace(
        media::cast::ToVideoDecoderConfig(receivers.video_config),
        receivers.video_receiver);
  }

  return {session, std::move(audio_stream_info), std::move(video_stream_info),
          /* is_remoting = */ false};
}

}  // namespace

CastStreamingSession::ReceiverSessionClient::ReceiverSessionClient(
    CastStreamingSession::Client* client,
    std::optional<RendererControllerConfig> renderer_controls,
    ReceiverConfig av_constraints,
    ReceiverSession::MessagePortProvider message_port_provider,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner),
      environment_(&openscreen::Clock::now, task_runner_),
      cast_message_port_converter_(CastMessagePortConverter::Create(
          std::move(message_port_provider),
          base::BindOnce(
              &CastStreamingSession::ReceiverSessionClient::OnCastChannelClosed,
              base::Unretained(this)))),
      client_(client),
      weak_factory_(this) {
  DCHECK(task_runner);
  DCHECK(client_);

  // This will fail if the "trivial" implementation of
  // CastMessagePortConverter::Create is linked.
  DCHECK(cast_message_port_converter_);

  receiver_session_ = std::make_unique<openscreen::cast::ReceiverSession>(
      *this, environment_, cast_message_port_converter_->GetMessagePort(),
      ToOpenscreenConstraints(av_constraints));

  if (renderer_controls) {
    playback_command_dispatcher_ = std::make_unique<PlaybackCommandDispatcher>(
        task_runner, std::move(renderer_controls.value().control_configuration),
        base::BindRepeating(
            &CastStreamingSession::ReceiverSessionClient::OnFlushUntil,
            weak_factory_.GetWeakPtr()),
        std::move(av_constraints.remoting));
    playback_command_dispatcher_->RegisterCommandSource(
        std::move(renderer_controls.value().external_renderer_controls));
  }

  init_timeout_timer_.Start(
      FROM_HERE, kInitTimeout,
      base::BindOnce(
          &CastStreamingSession::ReceiverSessionClient::OnInitializationTimeout,
          base::Unretained(this)));
}

void CastStreamingSession::ReceiverSessionClient::GetAudioBuffer(
    base::OnceClosure no_frames_available_cb) {
  if (preloaded_audio_buffer_) {
    DCHECK(preloaded_audio_buffer_.value());
    client_->OnAudioBufferReceived(std::move(preloaded_audio_buffer_.value()));
    preloaded_audio_buffer_ = std::nullopt;
    return;
  }

  DCHECK(audio_consumer_);
  audio_consumer_->ReadFrame(std::move(no_frames_available_cb));
}

void CastStreamingSession::ReceiverSessionClient::GetVideoBuffer(
    base::OnceClosure no_frames_available_cb) {
  if (preloaded_video_buffer_) {
    DCHECK(preloaded_video_buffer_.value());
    client_->OnVideoBufferReceived(std::move(preloaded_video_buffer_.value()));
    preloaded_video_buffer_ = std::nullopt;
    return;
  }

  DCHECK(video_consumer_);
  video_consumer_->ReadFrame(std::move(no_frames_available_cb));
}

void CastStreamingSession::ReceiverSessionClient::PreloadAudioBuffer(
    media::mojom::DecoderBufferPtr buffer) {
  DCHECK(!preloaded_audio_buffer_);
  DCHECK(buffer);

  DVLOG(1) << "Audio buffer preloaded!";

  preloaded_audio_buffer_ = std::move(buffer);
  if (playback_command_dispatcher_ && !ongoing_session_has_video()) {
    playback_command_dispatcher_->TryStartPlayback(
        (*preloaded_audio_buffer_)->timestamp);
  }
}

void CastStreamingSession::ReceiverSessionClient::PreloadVideoBuffer(
    media::mojom::DecoderBufferPtr buffer) {
  DCHECK(!preloaded_video_buffer_);
  DCHECK(buffer);

  DVLOG(1) << "Video buffer preloaded!";

  preloaded_video_buffer_ = std::move(buffer);
  if (playback_command_dispatcher_ && ongoing_session_has_video()) {
    playback_command_dispatcher_->TryStartPlayback(
        (*preloaded_video_buffer_)->timestamp);
  }
}

CastStreamingSession::ReceiverSessionClient::~ReceiverSessionClient() {
  // Teardown of the `receiver_session_` may trigger callbacks into `this`,
  // so destroy it explicitly here, so that callbacks execute while all other
  // members are still valid.
  receiver_session_.reset();
}

void CastStreamingSession::ReceiverSessionClient::OnInitializationTimeout() {
  DVLOG(1) << __func__;
  DCHECK(!is_initialized_);
  client_->OnSessionEnded();
  is_initialized_ = true;
}

std::optional<mojo::ScopedDataPipeConsumerHandle>
CastStreamingSession::ReceiverSessionClient::InitializeAudioConsumer(
    const StreamingInitializationInfo& initialization_info) {
  DCHECK(initialization_info.audio_stream_info);

  // Create the audio data pipe.
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  if (!CreateDataPipeForStreamType(media::DemuxerStream::Type::AUDIO,
                                   &data_pipe_producer, &data_pipe_consumer)) {
    return std::nullopt;
  }

  std::unique_ptr<DecoderBufferFactory> decoder_buffer_factory;
  if (initialization_info.is_remoting) {
    decoder_buffer_factory = std::make_unique<RemotingDecoderBufferFactory>();
  } else {
    // The duration is set to kNoTimestamp so the audio renderer does not block.
    // Audio frames duration is not known ahead of time in mirroring.
    decoder_buffer_factory = std::make_unique<MirroringDecoderBufferFactory>(
        initialization_info.audio_stream_info->receiver->rtp_timebase(),
        media::kNoTimestamp);
  }

  // We can use unretained pointers here because StreamConsumer is owned by
  // this object and |client_| is guaranteed to outlive this object.
  audio_consumer_ = std::make_unique<StreamConsumer>(
      initialization_info.audio_stream_info->receiver,
      std::move(data_pipe_producer),
      base::BindRepeating(&CastStreamingSession::Client::OnAudioBufferReceived,
                          base::Unretained(client_)),
      base::BindRepeating(&base::OneShotTimer::Reset,
                          base::Unretained(&data_timeout_timer_)),
      std::move(decoder_buffer_factory));

  return data_pipe_consumer;
}

std::optional<mojo::ScopedDataPipeConsumerHandle>
CastStreamingSession::ReceiverSessionClient::InitializeVideoConsumer(
    const StreamingInitializationInfo& initialization_info) {
  DCHECK(initialization_info.video_stream_info);

  // Create the video data pipe.
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  if (!CreateDataPipeForStreamType(media::DemuxerStream::Type::VIDEO,
                                   &data_pipe_producer, &data_pipe_consumer)) {
    return std::nullopt;
  }

  std::unique_ptr<DecoderBufferFactory> decoder_buffer_factory;
  if (initialization_info.is_remoting) {
    decoder_buffer_factory = std::make_unique<RemotingDecoderBufferFactory>();
  } else {
    // The frame duration is set to 10 minutes to work around cases where
    // senders do not send data for a long period of time. We end up with
    // overlapping video frames but this is fine since the media pipeline mostly
    // considers the playout time when deciding which frame to present or play
    decoder_buffer_factory = std::make_unique<MirroringDecoderBufferFactory>(
        initialization_info.video_stream_info->receiver->rtp_timebase(),
        base::Minutes(10));
  }

  // We can use unretained pointers here because StreamConsumer is owned by
  // this object and |client_| is guaranteed to outlive this object.
  // |data_timeout_timer_| is also owned by this object and will outlive both
  // StreamConsumers.
  video_consumer_ = std::make_unique<StreamConsumer>(
      initialization_info.video_stream_info->receiver,
      std::move(data_pipe_producer),
      base::BindRepeating(&CastStreamingSession::Client::OnVideoBufferReceived,
                          base::Unretained(client_)),
      base::BindRepeating(&base::OneShotTimer::Reset,
                          base::Unretained(&data_timeout_timer_)),
      std::move(decoder_buffer_factory));

  return data_pipe_consumer;
}

void CastStreamingSession::ReceiverSessionClient::StartStreamingSession(
    StreamingInitializationInfo initialization_info) {
  DVLOG(1) << __func__;
  DCHECK_EQ(initialization_info.session, receiver_session_.get());
  DCHECK(!initialization_info.is_remoting || IsCastRemotingEnabled());

  // If a Flush() call is ongoing, its unsafe to begin streaming data, so
  // instead stall this call until the Flush() call has completed.
  DCHECK(!start_session_cb_);
  if (is_flush_pending_) {
    start_session_cb_ = base::BindOnce(
        &CastStreamingSession::ReceiverSessionClient::StartStreamingSession,
        weak_factory_.GetWeakPtr(), std::move(initialization_info));
    return;
  }

  // If audio is not supported on this receiver, disable it to avoid AV-sync
  // issues arising from waiting for audio frames before starting playback.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioOutput)) {
    LOG(WARNING) << "Disabling audio for this session due to non-support by "
                    "the hosting product instance";
    initialization_info.audio_stream_info = std::nullopt;
  }

  // This is necessary in case the offer message had no audio and no video
  // stream.
  if (!initialization_info.audio_stream_info &&
      !initialization_info.video_stream_info) {
    client_->OnSessionEnded();
    return;
  }

  init_timeout_timer_.Stop();

  bool is_new_offer = is_initialized_;
  if (is_new_offer) {
    // This is a second offer message, reinitialize the streams.
    const bool new_offer_has_audio = !!initialization_info.audio_stream_info;
    const bool new_offer_has_video = !!initialization_info.video_stream_info;

    if (new_offer_has_audio != ongoing_session_has_audio() ||
        new_offer_has_video != ongoing_session_has_video()) {
      // This call to StartStreamingSession() has support for audio and/or video
      // streaming which does not match the ones provided during a prior call to
      // this method. Return early here.
      DLOG(ERROR) << "New streaming session has support for audio or video "
                     "which does not match the ones provided during a prior "
                     "streaming initialization.";
      client_->OnSessionEnded();
      return;
    }
  }

  // Set |is_initialized_| now so we can return early on failure.
  is_initialized_ = true;

  std::optional<mojo::ScopedDataPipeConsumerHandle> audio_pipe_consumer_handle;
  if (initialization_info.audio_stream_info) {
    audio_pipe_consumer_handle = InitializeAudioConsumer(initialization_info);
    if (audio_pipe_consumer_handle) {
      DVLOG(1) << "Initialized audio stream. "
               << initialization_info.audio_stream_info->config
                      .AsHumanReadableString();
    } else {
      client_->OnSessionEnded();
      return;
    }
  }

  std::optional<mojo::ScopedDataPipeConsumerHandle> video_pipe_consumer_handle;
  if (initialization_info.video_stream_info) {
    video_pipe_consumer_handle = InitializeVideoConsumer(initialization_info);
    if (video_pipe_consumer_handle) {
      DVLOG(1) << "Initialized video stream. "
               << initialization_info.video_stream_info->config
                      .AsHumanReadableString();
    } else {
      audio_consumer_.reset();
      client_->OnSessionEnded();
      return;
    }
  }

  if (is_new_offer) {
    client_->OnSessionReinitialization(std::move(initialization_info),
                                       std::move(audio_pipe_consumer_handle),
                                       std::move(video_pipe_consumer_handle));
  } else {
    client_->OnSessionInitialization(std::move(initialization_info),
                                     std::move(audio_pipe_consumer_handle),
                                     std::move(video_pipe_consumer_handle));
    data_timeout_timer_.Start(
        FROM_HERE, kNoDataTimeout,
        base::BindOnce(
            &CastStreamingSession::ReceiverSessionClient::OnDataTimeout,
            base::Unretained(this)));
  }
}

void CastStreamingSession::ReceiverSessionClient::OnNegotiated(
    const openscreen::cast::ReceiverSession* session,
    openscreen::cast::ReceiverSession::ConfiguredReceivers receivers) {
  StartStreamingSession(
      CreateMirroringInitializationInfo(session, std::move(receivers)));
}

void CastStreamingSession::ReceiverSessionClient::OnRemotingNegotiated(
    const openscreen::cast::ReceiverSession* session,
    openscreen::cast::ReceiverSession::RemotingNegotiation negotiation) {
  DCHECK(playback_command_dispatcher_);
  playback_command_dispatcher_->OnRemotingSessionNegotiated(
      negotiation.messenger);
  playback_command_dispatcher_->ConfigureRemotingAsync(
      this, session, std::move(negotiation.receivers));
}

void CastStreamingSession::ReceiverSessionClient::OnReceiversDestroying(
    const openscreen::cast::ReceiverSession* session,
    ReceiversDestroyingReason reason) {
  // This can be called when |receiver_session_| is being destroyed, so we
  // do not sanity-check |session| here.
  DVLOG(1) << __func__;
  if (playback_command_dispatcher_) {
    playback_command_dispatcher_->OnRemotingSessionEnded();
  }

  preloaded_audio_buffer_ = std::nullopt;
  preloaded_video_buffer_ = std::nullopt;

  switch (reason) {
    case ReceiversDestroyingReason::kEndOfSession:
      client_->OnSessionEnded();
      break;
    case ReceiversDestroyingReason::kRenegotiated:
      if (playback_command_dispatcher_) {
        if (is_flush_pending_) {
          DLOG(WARNING)
              << "Skipping call to Flush() because one is already in progress";
        } else {
          DVLOG(1) << "Calling Flush()";
          is_flush_pending_ = true;
          playback_command_dispatcher_->Flush(base::BindOnce(
              &CastStreamingSession::ReceiverSessionClient::OnFlushComplete,
              weak_factory_.GetWeakPtr()));
        }
      }
      client_->OnSessionReinitializationPending();
      break;
  }
}

void CastStreamingSession::ReceiverSessionClient::OnFlushComplete() {
  DCHECK(is_flush_pending_);

  DVLOG(1) << "Flush() Complete!";
  is_flush_pending_ = false;
  if (start_session_cb_) {
    std::move(start_session_cb_).Run();
  }
}

void CastStreamingSession::ReceiverSessionClient::OnFlushUntil(
    uint32_t audio_count,
    uint32_t video_count) {
  DVLOG(1) << "OnFlushUntil called: (audio_count=" << audio_count
           << ", video_count=" << video_count << ")";
  if (audio_consumer_) {
    audio_consumer_->FlushUntil(audio_count);
  }
  if (video_consumer_) {
    video_consumer_->FlushUntil(video_count);
  }
}

void CastStreamingSession::ReceiverSessionClient::OnError(
    const openscreen::cast::ReceiverSession* session,
    const openscreen::Error& error) {
  DCHECK_EQ(session, receiver_session_.get());
  LOG(ERROR) << error;
  if (!is_initialized_) {
    client_->OnSessionEnded();
    is_initialized_ = true;
  }
}

void CastStreamingSession::ReceiverSessionClient::OnDataTimeout() {
  DLOG(ERROR) << __func__ << ": Session ended due to timeout";
  receiver_session_.reset();
  client_->OnSessionEnded();
}

void CastStreamingSession::ReceiverSessionClient::OnCastChannelClosed() {
  DLOG(ERROR) << __func__ << ": Session ended due to cast channel closure";
  receiver_session_.reset();
  client_->OnSessionEnded();
}

base::WeakPtr<CastStreamingSession::ReceiverSessionClient>
CastStreamingSession::ReceiverSessionClient::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

CastStreamingSession::Client::~Client() = default;
CastStreamingSession::CastStreamingSession() = default;
CastStreamingSession::~CastStreamingSession() = default;

void CastStreamingSession::Start(
    Client* client,
    std::optional<RendererControllerConfig> renderer_controls,
    ReceiverConfig av_constraints,
    ReceiverSession::MessagePortProvider message_port_provider,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DVLOG(1) << __func__;
  DCHECK(client);
  DCHECK(!receiver_session_);
  receiver_session_ = std::make_unique<ReceiverSessionClient>(
      client, std::move(renderer_controls), std::move(av_constraints),
      std::move(message_port_provider), task_runner);
}

void CastStreamingSession::Stop() {
  DVLOG(1) << __func__;
  DCHECK(receiver_session_);
  receiver_session_.reset();
}

AudioDemuxerStreamDataProvider::RequestBufferCB
CastStreamingSession::GetAudioBufferRequester() {
  DCHECK(receiver_session_);
  return base::BindRepeating(
      &CastStreamingSession::ReceiverSessionClient::GetAudioBuffer,
      receiver_session_->GetWeakPtr());
}

VideoDemuxerStreamDataProvider::RequestBufferCB
CastStreamingSession::GetVideoBufferRequester() {
  DCHECK(receiver_session_);
  return base::BindRepeating(
      &CastStreamingSession::ReceiverSessionClient::GetVideoBuffer,
      receiver_session_->GetWeakPtr());
}

CastStreamingSession::PreloadBufferCB
CastStreamingSession::GetAudioBufferPreloader() {
  DCHECK(receiver_session_);
  return base::BindRepeating(
      &CastStreamingSession::ReceiverSessionClient::PreloadAudioBuffer,
      receiver_session_->GetWeakPtr());
}

CastStreamingSession::PreloadBufferCB
CastStreamingSession::GetVideoBufferPreloader() {
  DCHECK(receiver_session_);
  return base::BindRepeating(
      &CastStreamingSession::ReceiverSessionClient::PreloadVideoBuffer,
      receiver_session_->GetWeakPtr());
}

}  // namespace cast_streaming
