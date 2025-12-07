// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_PLAYBACK_COMMAND_DISPATCHER_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_PLAYBACK_COMMAND_DISPATCHER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/browser/common/streaming_initialization_info.h"
#include "components/cast_streaming/browser/control/remoting/remoting_session_client.h"
#include "components/cast_streaming/browser/control/remoting/renderer_rpc_call_translator.h"
#include "components/cast_streaming/browser/control/remoting/rpc_demuxer_stream_handler.h"
#include "components/cast_streaming/browser/control/remoting/rpc_initialization_call_handler_base.h"
#include "components/cast_streaming/browser/control/renderer_control_multiplexer.h"
#include "components/cast_streaming/browser/public/receiver_config.h"
#include "components/cast_streaming/common/public/mojom/renderer_controller.mojom.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver_session.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"

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
// media::Renderer (expected to be a PlaybackCommandForwardingRenderer) via an
// initial call to |control_configuration| and then setting up any necessary
// infrastructure for messages to be passed across this pipe. While this class
// is used to initiate and maintain control over a Renderer for a Cast Remoting
// session, it is also used for starting playback of a Cast Mirroring session.
class PlaybackCommandDispatcher
    : public remoting::RpcInitializationCallHandlerBase,
      public remoting::RemotingSessionClient,
      public remoting::RpcDemuxerStreamHandler::Client {
 public:
  // Creates a new PlaybackCommandDispatcher.
  // |flush_until_cb| will be called when a Flush() remoting command is
  // received, if remoting is enabled.
  // |remoting_constraints| will be validated against received config values for
  // configs received when configuring the remoting stream, if remoting is
  // enabled.
  PlaybackCommandDispatcher(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      mojo::AssociatedRemote<mojom::RendererController> control_configuration,
      remoting::RendererRpcCallTranslator::FlushUntilCallback flush_until_cb,
      std::optional<ReceiverConfig::RemotingConstraints> remoting_constraints);
  ~PlaybackCommandDispatcher() override;

  void RegisterCommandSource(
      mojo::PendingReceiver<media::mojom::Renderer> controls);

  // Call Flush() on the Renderer associated with this streaming session.
  void Flush(media::mojom::Renderer::FlushCallback callback);

  // Begins playback of the streaming session via calls to the Renderer if it
  // has not yet begun. This is required because the remote device will
  // sometimes, but not always, call StartPlayingFrom() on a session that it
  // wants to be playing.
  void TryStartPlayback(base::TimeDelta timestamp);

  // remoting::RemotingSessionClient overrides.
  void OnRemotingSessionNegotiated(
      openscreen::cast::RpcMessenger* messenger) override;
  void ConfigureRemotingAsync(
      Dispatcher* dispatcher,
      const openscreen::cast::ReceiverSession* session,
      openscreen::cast::ReceiverSession::ConfiguredReceivers receivers)
      override;
  void OnRemotingSessionEnded() override;

 private:
  void SendRemotingRpcMessageToRemote(
      openscreen::cast::RpcMessenger::Handle handle,
      std::unique_ptr<openscreen::cast::RpcMessage> message);
  void ProcessRemotingRpcMessageFromRemote(
      std::unique_ptr<openscreen::cast::RpcMessage> message);

  // Acquires a new handle from |messenger_|.
  openscreen::cast::RpcMessenger::Handle AcquireHandle();

  // Registers a |handle| with |messenger_| to receive callbacks to
  // ProcessRemotingRpcMessageFromRemote().
  void RegisterHandleForCallbacks(
      openscreen::cast::RpcMessenger::Handle handle);

  // Starts streaming if each expected audio or video config has been received.
  void MaybeStartStreamingSession();

  // Callback for mojom::RendererController::SetPlaybackController() call.
  void OnSetPlaybackControllerDone();

  // RpcInitializationCallHandlerBase overrides.
  void RpcAcquireRendererAsync(
      openscreen::cast::RpcMessenger::Handle remote_handle,
      AcquireRendererCB cb) override;
  void OnRpcAcquireDemuxer(
      openscreen::cast::RpcMessenger::Handle audio_stream_handle,
      openscreen::cast::RpcMessenger::Handle video_stream_handle) override;

  // RpcDemuxerStreamHandler::Client overrides.
  void OnNewAudioConfig(media::AudioDecoderConfig new_config) override;
  void OnNewVideoConfig(media::VideoDecoderConfig new_config) override;

  // Synchronization for calling |acquire_renderer_cb_| at the correct time.
  bool has_set_playback_controller_call_returned_ = false;
  base::OnceCallback<void()> acquire_renderer_cb_;

  raw_ptr<openscreen::cast::RpcMessenger> messenger_;

  // Multiplexes Renderer commands from a number of senders.
  std::unique_ptr<RendererControlMultiplexer> muxer_;

  // Handles translating between Remoting commands (in proto form) and mojo
  // commands.
  std::unique_ptr<remoting::RendererRpcCallTranslator>
      renderer_call_translator_;

  // Handles DemuxerStream interactions.
  std::unique_ptr<remoting::RpcDemuxerStreamHandler> demuxer_stream_handler_;

  // Handles for the demuxer stream data providers, to be used for dispatching
  // demuxer stream RPC commands.
  std::optional<StreamingInitializationInfo> streaming_init_info_;
  std::optional<ReceiverConfig::RemotingConstraints> remoting_constraints_;
  raw_ptr<Dispatcher> streaming_dispatcher_ = nullptr;
  raw_ptr<const openscreen::cast::ReceiverSession> receiver_session_ = nullptr;

  // The mojo API used to configure the renderer controls in the renderer
  // process. Although this instance is only needed once, it is stored as an
  // instance variable so that the destruction of this instance is visible to
  // the Renderer process via the mojo disconnection handler.
  mojo::AssociatedRemote<mojom::RendererController> control_configuration_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<PlaybackCommandDispatcher> weak_factory_{this};
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_PLAYBACK_COMMAND_DISPATCHER_H_
