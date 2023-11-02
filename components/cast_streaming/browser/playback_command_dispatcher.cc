// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/playback_command_dispatcher.h"

#include "base/bind.h"
#include "base/task/bind_post_task.h"
#include "components/cast_streaming/browser/renderer_rpc_call_translator.h"
#include "components/cast_streaming/public/rpc_call_message_handler.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace cast_streaming {

PlaybackCommandDispatcher::PlaybackCommandDispatcher(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::AssociatedRemote<mojom::RendererController> control_configuration)
    : RpcInitializationCallHandlerBase(base::BindRepeating(
          &PlaybackCommandDispatcher::SendRemotingRpcMessageToRemote,
          base::Unretained(this))),
      task_runner_(std::move(task_runner)),
      weak_factory_(this) {
  // Create a muxer using the "real" media::mojom::Renderer instance that
  // connects to the remote media::Renderer.
  mojo::Remote<media::mojom::Renderer> renderer;
  control_configuration_ = std::move(control_configuration);
  control_configuration_->SetPlaybackController(
      renderer.BindNewPipeAndPassReceiver(),
      base::BindOnce(&PlaybackCommandDispatcher::OnSetPlaybackControllerDone,
                     weak_factory_.GetWeakPtr()));
  muxer_ = std::make_unique<RendererControlMultiplexer>(std::move(renderer),
                                                        task_runner_);

  auto message_processor_callback = base::BindRepeating(
      &PlaybackCommandDispatcher::SendRemotingRpcMessageToRemote,
      weak_factory_.GetWeakPtr());
  renderer_call_translator_ =
      std::make_unique<remoting::RendererRpcCallTranslator>(
          std::move(message_processor_callback), muxer_.get());
}

PlaybackCommandDispatcher::~PlaybackCommandDispatcher() {
  OnRemotingSessionEnded();
}

void PlaybackCommandDispatcher::RegisterCommandSource(
    mojo::PendingReceiver<media::mojom::Renderer> controls) {
  muxer_->RegisterController(std::move(controls));
}

void PlaybackCommandDispatcher::OnRemotingSessionNegotiated(
    openscreen::cast::RpcMessenger* messenger) {
  DCHECK(messenger);

  messenger_ = messenger;
  RegisterHandleForCallbacks(
      openscreen::cast::RpcMessenger::kAcquireRendererHandle);
  RegisterHandleForCallbacks(
      openscreen::cast::RpcMessenger::kAcquireDemuxerHandle);

  renderer_call_translator_->set_handle(AcquireHandle());
  demuxer_stream_handler_ = std::make_unique<remoting::RpcDemuxerStreamHandler>(
      this,
      base::BindRepeating(&PlaybackCommandDispatcher::AcquireHandle,
                          base::Unretained(this)),
      base::BindRepeating(
          &PlaybackCommandDispatcher::SendRemotingRpcMessageToRemote,
          base::Unretained(this)));
}

void PlaybackCommandDispatcher::ConfigureRemotingAsync(
    Dispatcher* dispatcher,
    const openscreen::cast::ReceiverSession* session,
    openscreen::cast::ReceiverSession::ConfiguredReceivers receivers) {
  DCHECK(dispatcher);
  DCHECK(session);
  DCHECK(demuxer_stream_handler_);
  DCHECK(!streaming_init_info_);

  streaming_dispatcher_ = dispatcher;
  receiver_session_ = session;

  absl::optional<StreamingInitializationInfo::AudioStreamInfo>
      audio_stream_info;
  if (receivers.audio_receiver) {
    audio_stream_info.emplace(media::AudioDecoderConfig(),
                              receivers.audio_receiver);
  }

  absl::optional<StreamingInitializationInfo::VideoStreamInfo>
      video_stream_info;
  if (receivers.video_receiver) {
    video_stream_info.emplace(media::VideoDecoderConfig(),
                              receivers.video_receiver);
  }

  streaming_init_info_.emplace(receiver_session_, std::move(audio_stream_info),
                               std::move(video_stream_info));
}

void PlaybackCommandDispatcher::OnRemotingSessionEnded() {
  demuxer_stream_handler_.reset();
  messenger_ = nullptr;
  streaming_init_info_ = absl::nullopt;
}

void PlaybackCommandDispatcher::SendRemotingRpcMessageToRemote(
    openscreen::cast::RpcMessenger::Handle handle,
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK_NE(handle, openscreen::cast::RpcMessenger::kInvalidHandle);
  DCHECK(message);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!messenger_) {
    return;
  }

  message->set_handle(handle);
  messenger_->SendMessageToRemote(*message);
}

void PlaybackCommandDispatcher::ProcessRemotingRpcMessageFromRemote(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(message);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool did_dispatch_as_initialization_call =
      remoting::DispatchInitializationRpcCall(message.get(), this);
  if (did_dispatch_as_initialization_call) {
    return;
  }

  const bool did_dispatch_as_renderer_call =
      renderer_call_translator_ &&
      remoting::DispatchRendererRpcCall(message.get(),
                                        renderer_call_translator_.get());
  if (did_dispatch_as_renderer_call) {
    return;
  }

  const bool did_dispatch_as_demuxer_stream_callback =
      demuxer_stream_handler_ &&
      remoting::DispatchDemuxerStreamCBRpcCall(message.get(),
                                               demuxer_stream_handler_.get());
  if (did_dispatch_as_demuxer_stream_callback) {
    return;
  }

  LOG(ERROR) << "Unhandled RPC Message for command " << message->proc();
}

openscreen::cast::RpcMessenger::Handle
PlaybackCommandDispatcher::AcquireHandle() {
  DCHECK(messenger_);
  auto handle = messenger_->GetUniqueHandle();
  RegisterHandleForCallbacks(handle);
  return handle;
}

void PlaybackCommandDispatcher::RegisterHandleForCallbacks(
    openscreen::cast::RpcMessenger::Handle handle) {
  DCHECK(messenger_);
  messenger_->RegisterMessageReceiverCallback(
      handle, [ptr = weak_factory_.GetWeakPtr()](
                  std::unique_ptr<openscreen::cast::RpcMessage> message) {
        if (!ptr) {
          DVLOG(1)
              << "Message receiver has been invalidated. Dropping message.";
          return;
        }
        ptr->ProcessRemotingRpcMessageFromRemote(std::move(message));
      });
}

void PlaybackCommandDispatcher::OnSetPlaybackControllerDone() {
  has_set_playback_controller_call_returned_ = true;

  if (acquire_renderer_cb_) {
    std::move(acquire_renderer_cb_).Run();
  }
}

void PlaybackCommandDispatcher::RpcAcquireRendererAsync(AcquireRendererCB cb) {
  DCHECK(renderer_call_translator_);
  const auto handle = renderer_call_translator_->handle();

  DCHECK_NE(handle, openscreen::cast::RpcMessenger::kInvalidHandle);
  acquire_renderer_cb_ = base::BindOnce(std::move(cb), handle);

  if (has_set_playback_controller_call_returned_) {
    std::move(acquire_renderer_cb_).Run();
  }
}

void PlaybackCommandDispatcher::OnRpcAcquireDemuxer(
    openscreen::cast::RpcMessenger::Handle audio_stream_handle,
    openscreen::cast::RpcMessenger::Handle video_stream_handle) {
  if (demuxer_stream_handler_) {
    demuxer_stream_handler_->OnRpcAcquireDemuxer(audio_stream_handle,
                                                 video_stream_handle);
  }
}

void PlaybackCommandDispatcher::OnNewAudioConfig(
    media::AudioDecoderConfig config) {
  DCHECK(streaming_init_info_);
  DCHECK(streaming_dispatcher_);
  if (!streaming_init_info_->audio_stream_info) {
    LOG(ERROR) << "Received Audio config for a remoting session where audio is "
                  "not supported";
    return;
  }

  streaming_init_info_->audio_stream_info->config = std::move(config);
  MaybeStartStreamingSession();
}

void PlaybackCommandDispatcher::OnNewVideoConfig(
    media::VideoDecoderConfig config) {
  DCHECK(streaming_init_info_);
  DCHECK(streaming_dispatcher_);
  if (!streaming_init_info_->video_stream_info) {
    LOG(ERROR) << "Received Video config for a remoting session where video is "
                  "not supported";
    return;
  }

  streaming_init_info_->video_stream_info->config = std::move(config);
  MaybeStartStreamingSession();
}

void PlaybackCommandDispatcher::MaybeStartStreamingSession() {
  DCHECK(streaming_init_info_);
  const bool is_audio_config_ready =
      !streaming_init_info_->audio_stream_info ||
      !streaming_init_info_->audio_stream_info->config.Matches(
          media::AudioDecoderConfig());
  const bool is_video_config_ready =
      !streaming_init_info_->video_stream_info ||
      !streaming_init_info_->video_stream_info->config.Matches(
          media::VideoDecoderConfig());
  if (!is_audio_config_ready || !is_video_config_ready) {
    return;
  }

  DCHECK(demuxer_stream_handler_);
  if (streaming_init_info_->audio_stream_info) {
    auto client = demuxer_stream_handler_->GetAudioClient();
    DCHECK(client);
    streaming_init_info_->audio_stream_info->demuxer_stream_client =
        std::move(client);
  }
  if (streaming_init_info_->video_stream_info) {
    auto client = demuxer_stream_handler_->GetVideoClient();
    DCHECK(client);
    streaming_init_info_->video_stream_info->demuxer_stream_client =
        std::move(client);
  }

  // |streaming_init_info_| is intentionally copied here.
  DCHECK(streaming_dispatcher_);
  streaming_dispatcher_->StartStreamingSession(streaming_init_info_.value());
}

}  // namespace cast_streaming
