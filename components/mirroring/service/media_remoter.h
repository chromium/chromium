// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MEDIA_REMOTER_H_
#define COMPONENTS_MIRRORING_SERVICE_MEDIA_REMOTER_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace mirroring {

class RpcDispatcher;

// MediaRemoter remotes media content directly to a Cast Receiver. When
// MediaRemoter is started, it connects itself with a source tab in browser
// through the Mirroring Service mojo interface and allows the browser to access
// this MediaRemoter to start/stop individual remoting sessions, which are
// caused by user actions (i.e., when they somehow indicate a desire to
// enter/leave an immersive video-watching mode).
//
// When a remoting session is started, MediaRemoter will first request that tab
// mirroring be switched into content remoting mode. If granted, it will notify
// the browser that this has succeeded. At this point, two-way RPC binary
// messaging begins, and the MediaRemoter simply forwards messages between the
// browser and the Cast Receiver. The audio/video data streams are delivered
// from the media renderer to the Mirroring Service through mojo datapipes, and
// are then sent out to Cast Receiver through Cast Streaming.
class COMPONENT_EXPORT(MIRRORING_SERVICE) MediaRemoter final
    : public media::mojom::Remoter {
 public:
  class Client {
   public:
    virtual ~Client() = default;

    // Connects the |remoter| with a source tab.
    virtual void ConnectToRemotingSource(
        mojo::PendingRemote<media::mojom::Remoter> remoter,
        mojo::PendingReceiver<media::mojom::RemotingSource>
            source_receiver) = 0;

    // Requests to start remoting. OnRemotingStarted() / OnRemotingStartFailed()
    // will be called when starting succeeds / fails.
    virtual void RequestRemotingStreaming() = 0;

    // Requests to resume mirroring.
    virtual void RestartMirroringStreaming() = 0;

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
               RpcDispatcher& message_dispatcher);

  MediaRemoter(const MediaRemoter&) = delete;
  MediaRemoter& operator=(const MediaRemoter&) = delete;

  ~MediaRemoter() override;

  // Callback from |message_dispatcher_| for received RPC messages.
  void OnMessageFromSink(const std::vector<uint8_t>& response);

  // Called when OFFER/ANSWER exchange for a remoting session succeeds.
  void OnRemotingStarted();

  // Called when a mirroring session is successfully resumed.
  void OnMirroringResumed(bool is_tab_switching = false);

  // Error occurred either during the start of remoting or in the middle of
  // remoting. In either case, this call fallbacks to mirroring, and prevents
  // further starting of media remoting during this mirroring session.
  void OnRemotingFailed();

  // media::mojom::Remoter implementation.
  // Stops the current remoting session with a given |reason|.
  void Stop(media::mojom::RemotingStopReason reason) override;

 private:
  // media::mojom::Remoter implementation.
  void Start() override;
  void StartWithPermissionAlreadyGranted() override;
  void StartDataStreams(
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          audio_sender_receiver,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          video_sender_receiver) override;
  void SendMessageToSink(const std::vector<uint8_t>& message) override;
  void EstimateTransmissionCapacity(
      media::mojom::Remoter::EstimateTransmissionCapacityCallback callback)
      override;
  // Called by RemotingDataStreamSender when error occurred. Will stop this
  // remoting session and fallback to mirroring.
  void OnRemotingDataStreamError();

  raw_ref<Client> client_;
  const media::mojom::RemotingSinkMetadata sink_metadata_;
  raw_ref<RpcDispatcher> rpc_dispatcher_;
  mojo::Receiver<media::mojom::Remoter> receiver_{this};
  mojo::Remote<media::mojom::RemotingSource> remoting_source_;
  std::unique_ptr<media::mojom::RemotingDataStreamSender> audio_sender_;
  std::unique_ptr<media::mojom::RemotingDataStreamSender> video_sender_;

  // State transition diagram:
  //
  // .-----------> MIRRORING
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
    MIRRORING,          // In mirroring.
    STARTING_REMOTING,  // Starting a remoting session.
    REMOTING_STARTED,   // Remoting started successfully.
    REMOTING_DISABLED,  // Remoting was disabled (because of error).
    STOPPING_REMOTING,  // Stopping the remoting session.
  } state_;

  base::WeakPtrFactory<MediaRemoter> weak_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MEDIA_REMOTER_H_
