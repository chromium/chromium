// Copyright 2021 The Chromium Authors. All rights reserved.
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
  control_configuration->SetPlaybackController(
      renderer.BindNewPipeAndPassReceiver(),
      base::BindOnce(&PlaybackCommandDispatcher::OnSetPlaybackControllerDone,
                     weak_factory_.GetWeakPtr()));
  muxer_ = std::make_unique<RendererControlMultiplexer>(std::move(renderer),
                                                        task_runner_);

  // Create a "fake" media::mojom::Renderer so that the RpcCallTranslator can
  // pass commands to the |muxer_|.
  mojo::Remote<media::mojom::Renderer> translators_renderer;
  RegisterCommandSource(translators_renderer.BindNewPipeAndPassReceiver());
  call_translator_ = std::make_unique<remoting::RendererRpcCallTranslator>(
      std::move(translators_renderer));
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
  handle_ = messenger_->GetUniqueHandle();

  // Include the |handle_| in the callback so that it will persist even upon
  // re-negotiation.
  auto message_processor_callback = base::BindPostTask(
      task_runner_,
      base::BindRepeating(
          &PlaybackCommandDispatcher::SendRemotingRpcMessageToRemote,
          weak_factory_.GetWeakPtr(), handle_),
      FROM_HERE);
  call_translator_->SetMessageProcessor(std::move(message_processor_callback));

  auto message_receiver_callback = base::BindPostTask(
      task_runner_,
      base::BindRepeating(
          &PlaybackCommandDispatcher::ProcessRemotingRpcMessageFromRemote,
          weak_factory_.GetWeakPtr()),
      FROM_HERE);
  messenger_->RegisterMessageReceiverCallback(
      handle_, [cb = std::move(message_receiver_callback)](
                   std::unique_ptr<openscreen::cast::RpcMessage> message) {
        cb.Run(std::move(message));
      });
}

void PlaybackCommandDispatcher::OnRemotingSessionEnded() {
  if (messenger_) {
    messenger_->UnregisterMessageReceiverCallback(handle_);
    messenger_ = nullptr;
  }
}

void PlaybackCommandDispatcher::SendRemotingRpcMessageToRemote(
    openscreen::cast::RpcMessenger::Handle handle,
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
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

  // TODO(rwkeane): Handle DemuxerStream messages too.
  if (!remoting::DispatchInitializationRpcCall(message.get(), this) &&
      !remoting::DispatchRendererRpcCall(message.get(),
                                         call_translator_.get())) {
    LOG(ERROR) << "Unhandled RPC Message for command " << message->proc();
  }
}

void PlaybackCommandDispatcher::OnSetPlaybackControllerDone() {
  has_set_playback_controller_call_returned_ = true;

  if (acquire_renderer_cb_) {
    std::move(acquire_renderer_cb_).Run();
  }
}

void PlaybackCommandDispatcher::RpcAcquireRendererAsync(AcquireRendererCB cb) {
  acquire_renderer_cb_ = base::BindOnce(std::move(cb), handle_);

  if (has_set_playback_controller_call_returned_) {
    std::move(acquire_renderer_cb_).Run();
  }
}

void PlaybackCommandDispatcher::OnRpcAcquireDemuxer(int audio_stream_handle,
                                                    int video_stream_handle) {
  // TODO(rwkeane): Handle DemuxerStreams.
  NOTIMPLEMENTED();
}

}  // namespace cast_streaming
