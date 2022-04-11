// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_PLAYBACK_COMMAND_DISPATCHER_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_PLAYBACK_COMMAND_DISPATCHER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/browser/remoting_session_client.h"
#include "components/cast_streaming/browser/renderer_control_multiplexer.h"
#include "components/cast_streaming/browser/rpc_initialization_call_handler_base.h"
#include "components/cast_streaming/public/mojom/renderer_controller.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/openscreen/src/cast/streaming/rpc_messenger.h"

namespace openscreen {
namespace cast {
class RpcMessage;
}  // namespace cast
}  // namespace openscreen

namespace cast_streaming {

namespace remoting {
class RendererRpcCallTranslator;
}  // namespace remoting

// This class is responsible for initiating a mojo connection to a
// media::Renderer (expected to be a
// cast_streaming::PlaybackCommandForwardingRenderer) via an initial call to
// |control_configuration| and then setting up any necessary infrastructure for
// messages to be passed across this pipe. While this class is used to initiate
// and maintain control over a Renderer for a Cast Remoting session, it is
// also used for starting playback of a Cast Mirroring session.
class PlaybackCommandDispatcher
    : public remoting::RpcInitializationCallHandlerBase,
      public remoting::RemotingSessionClient {
 public:
  PlaybackCommandDispatcher(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      mojo::AssociatedRemote<mojom::RendererController> control_configuration);
  ~PlaybackCommandDispatcher() override;

  void RegisterCommandSource(
      mojo::PendingReceiver<media::mojom::Renderer> controls);

  // remoting::RemotingSessionClient overrides.
  void OnRemotingSessionNegotiated(
      openscreen::cast::RpcMessenger* messenger) override;
  void OnRemotingSessionEnded() override;

 private:
  void SendRemotingRpcMessageToRemote(
      openscreen::cast::RpcMessenger::Handle handle,
      std::unique_ptr<openscreen::cast::RpcMessage> message);
  void ProcessRemotingRpcMessageFromRemote(
      std::unique_ptr<openscreen::cast::RpcMessage> message);

  // Callback for mojom::RendererController::SetPlaybackController() call.
  void OnSetPlaybackControllerDone();

  // RpcInitializationCallHandlerBase overrides.
  void RpcAcquireRendererAsync(AcquireRendererCB cb) override;
  void OnRpcAcquireDemuxer(int audio_stream_handle,
                           int video_stream_handle) override;

  // Synchronization for calling |acquire_renderer_cb_| at the correct time.
  bool has_set_playback_controller_call_returned_ = false;
  base::OnceCallback<void()> acquire_renderer_cb_;

  openscreen::cast::RpcMessenger* messenger_;
  openscreen::cast::RpcMessenger::Handle handle_;

  // Multiplexes Renderer commands from a number of senders.
  std::unique_ptr<RendererControlMultiplexer> muxer_;

  // Handles translating between Remoting commands (in proto form) and mojo
  // commands.
  std::unique_ptr<remoting::RendererRpcCallTranslator> call_translator_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<PlaybackCommandDispatcher> weak_factory_{this};
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_PLAYBACK_COMMAND_DISPATCHER_H_
