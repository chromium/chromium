// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MEDIA_REMOTER_H_
#define COMPONENTS_MIRRORING_SERVICE_MEDIA_REMOTER_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace mirroring {

class RpcDispatcher;

// MediaRemoter is the Mirroring Service's end of the media::mojom::Remoter and
// media::mojom::RemotingSource interface pair. It tells a remoting source when
// a sink is available, relays RPC messages between the source and the Cast
// Receiver, and hands the source's data pipes to senders that its Client
// creates.
//
// MediaRemoter does not own the streaming session. Its Client does, and the
// Client decides what to stream when remoting is not active.
//
// Like the mojom interfaces, the API is split by direction:
//  - Commands go down from the source to the Client: StartRemoting() and
//    StopRemoting().
//  - Events go up from the Client to the source: the public On*() methods.
// An event from the Client never causes a StartRemoting() or StopRemoting()
// call back into the Client.
class COMPONENT_EXPORT(MIRRORING_SERVICE) MediaRemoter final
    : public media::mojom::Remoter {
 public:
  class Client {
   public:
    virtual ~Client() = default;

    // Passes both pipe ends to whatever provides the remoting source. The
    // client may drop them if there is no source, in which case the remoter
    // never receives a command.
    virtual void ConnectToRemotingSource(
        mojo::PendingRemote<media::mojom::Remoter> remoter,
        mojo::PendingReceiver<media::mojom::RemotingSource>
            source_receiver) = 0;

    // Starts a remoting streaming session. The client answers with
    // OnRemotingStarted() or OnRemotingStartFailed(), unless StopRemoting()
    // cancels the start first.
    virtual void StartRemoting() = 0;

    // Stops the remoting streaming session, or cancels a pending
    // StartRemoting(). Called when the source stops remoting, or when the
    // remoter fails to relay an RPC message or a data stream. The client may
    // destroy the remoter from inside this call.
    virtual void StopRemoting(media::mojom::RemotingStopReason reason) = 0;

    // Creates a RemotingDataStreamSender for the given data pipe. The client
    // is responsible for providing the transport-specific sender
    // implementation. Returns nullptr if the sender cannot be created (e.g.,
    // no valid config or transport sender for the given stream type).
    virtual std::unique_ptr<media::mojom::RemotingDataStreamSender>
    CreateRemotingDataStreamSender(
        bool is_audio,
        mojo::ScopedDataPipeConsumerHandle pipe,
        mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> receiver,
        base::OnceClosure error_callback) = 0;
  };

  MediaRemoter(Client& client,
               const media::mojom::RemotingSinkMetadata& sink_metadata,
               RpcDispatcher& rpc_dispatcher);

  MediaRemoter(const MediaRemoter&) = delete;
  MediaRemoter& operator=(const MediaRemoter&) = delete;

  // Tells the source that remoting stopped with ROUTE_TERMINATED if a session
  // is starting or active. Does not call the client.
  ~MediaRemoter() override;

  // Answers to Client::StartRemoting(). Both do nothing if the start was
  // cancelled. A failed start disables remoting for the lifetime of this
  // object.
  void OnRemotingStarted();
  void OnRemotingStartFailed();

  // The client stopped the remoting streaming session, or cancelled a pending
  // start, for its own reasons.
  void OnRemotingStopped(media::mojom::RemotingStopReason reason);

  // The client can accept another StartRemoting() after remoting stopped.
  // Does nothing if remoting has not stopped, or is disabled.
  void OnReadyForRemoting();

  // The remoting source changed. Drops the current source and connects to the
  // new one through Client::ConnectToRemotingSource(). Does nothing if remoting
  // is disabled.
  void OnSourceChanged();

 private:
  // media::mojom::Remoter implementation. Only the source calls these.
  void Start() override;
  void StartDataStreams(
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          audio_sender_receiver,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          video_sender_receiver) override;
  void Stop(media::mojom::RemotingStopReason reason) override;
  void SendMessageToSink(const std::vector<uint8_t>& message) override;

  // Callbacks from `rpc_dispatcher_` for received RPC messages and errors.
  void OnMessageFromSink(const std::vector<uint8_t>& response);
  void OnRpcError();

  // Called by a RemotingDataStreamSender when an error occurred.
  void OnRemotingDataStreamError();

  // Binds new pipes and passes the other ends to the client.
  void ConnectToSource();

  // Ends the active session or pending start, and tells the source that
  // remoting stopped. Returns false if there was nothing to end.
  bool EndRemoting(media::mojom::RemotingStopReason reason);

  // Ends the active session or pending start because of an error in the
  // remoter, disables remoting, and tells the client to stop.
  void FailRemoting(media::mojom::RemotingStopReason reason);

  raw_ref<Client> client_;
  const media::mojom::RemotingSinkMetadata sink_metadata_;
  raw_ref<RpcDispatcher> rpc_dispatcher_;
  mojo::Receiver<media::mojom::Remoter> receiver_{this};
  mojo::Remote<media::mojom::RemotingSource> remoting_source_;
  std::unique_ptr<media::mojom::RemotingDataStreamSender> audio_sender_;
  std::unique_ptr<media::mojom::RemotingDataStreamSender> video_sender_;

  // State transition diagram:
  //
  // .-------------> READY
  // |                 |
  // |                 V
  // |           STARTING_REMOTING
  // |                 |
  // |                 V
  // |   .-----------------------------.
  // |   |          |                  |
  // |   |          V                  V
  // |   |  REMOTING_STARTED ----> REMOTING_DISABLED
  // |   |          |
  // |   V          V
  // .--STOPPING_REMOTING
  enum {
    READY,              // Ready to start remoting.
    STARTING_REMOTING,  // Starting a remoting session.
    REMOTING_STARTED,   // Remoting started successfully.
    REMOTING_DISABLED,  // Remoting was disabled (because of error).
    STOPPING_REMOTING,  // Stopping the remoting session.
  } state_;

  base::WeakPtrFactory<MediaRemoter> weak_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MEDIA_REMOTER_H_
