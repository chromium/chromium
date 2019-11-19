// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MEDIA_REMOTER_H_
#define COMPONENTS_MIRRORING_SERVICE_MEDIA_REMOTER_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "media/cast/cast_config.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {
namespace cast {
class CastEnvironment;
class CastTransport;
}  // namespace cast
}  // namespace media

namespace mirroring {

class MessageDispatcher;
struct ReceiverResponse;
class RemotingSender;

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
    virtual ~Client() {}

    // Connects the |remoter| with a source tab.
    virtual void ConnectToRemotingSource(
        mojo::PendingRemote<media::mojom::Remoter> remoter,
        mojo::PendingReceiver<media::mojom::RemotingSource>
            source_receiver) = 0;

    // Requests to start remoting. StartRpcMessaging() / OnRemotingStartFailed()
    // will be called when starting succeeds / fails.
    virtual void RequestRemotingStreaming() = 0;

    // Requests to resume mirroring.
    virtual void RestartMirroringStreaming() = 0;
  };

  MediaRemoter(Client* client,
               const media::mojom::RemotingSinkMetadata& sink_metadata,
               MessageDispatcher* message_dispatcher);

  ~MediaRemoter() override;

  // Callback from |message_dispatcher_| for received RPC messages.
  void OnMessageFromSink(const ReceiverResponse& response);

  // Called when OFFER/ANSWER exchange for a remoting session succeeds.
  void StartRpcMessaging(
      scoped_refptr<media::cast::CastEnvironment> cast_environment,
      media::cast::CastTransport* transport,
      const media::cast::FrameSenderConfig& audio_config,
      const media::cast::FrameSenderConfig& video_config);

  // Called when a mirroring session is successfully resumed.
  void OnMirroringResumed();

  // Error occurred either during the start of remoting or in the middle of
  // remoting. In either case, this call fallbacks to mirroring, and prevents
  // further starting of media remoting during this mirroring session.
  void OnRemotingFailed();

  // media::mojom::Remoter implememtation. Stops the current remoting session.
  // This could be called either by the RemotingSource or the Session.
  void Stop(media::mojom::RemotingStopReason reason) override;

 private:
  // media::mojom::Remoter implememtation.
  void Start() override;
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

  // Called by RemotingSender when error occurred. Will stop this remoting
  // session and fallback to mirroring.
  void OnRemotingDataStreamError();

  Client* const client_;  // Outlives this class.
  const media::mojom::RemotingSinkMetadata sink_metadata_;
  MessageDispatcher* const message_dispatcher_;  // Outlives this class.
  mojo::Receiver<media::mojom::Remoter> receiver_{this};
  mojo::Remote<media::mojom::RemotingSource> remoting_source_;
  scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  std::unique_ptr<RemotingSender> audio_sender_;
  std::unique_ptr<RemotingSender> video_sender_;
  media::cast::CastTransport* transport_;  // Outlives this class;
  media::cast::FrameSenderConfig audio_config_;
  media::cast::FrameSenderConfig video_config_;

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

  DISALLOW_COPY_AND_ASSIGN(MediaRemoter);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MEDIA_REMOTER_H_
