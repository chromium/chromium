// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MEDIA_REMOTER_H_
#define COMPONENTS_MIRRORING_SERVICE_MEDIA_REMOTER_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "media/cast/cast_config.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"

namespace media::cast {
class CastEnvironment;
}  // namespace media::cast

namespace mirroring {

class RpcDispatcher;
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
    virtual ~Client() = default;

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

  MediaRemoter(Client& client,
               const media::mojom::RemotingSinkMetadata& sink_metadata,
               RpcDispatcher& message_dispatcher);

  MediaRemoter(const MediaRemoter&) = delete;
  MediaRemoter& operator=(const MediaRemoter&) = delete;

  ~MediaRemoter() override;

  // Callback from |message_dispatcher_| for received RPC messages.
  void OnMessageFromSink(const std::vector<uint8_t>& response);

  // Called when OFFER/ANSWER exchange for a remoting session succeeds.
  // New way using openscreen::cast::Sender objects.
  // NOTE: either `audio_sender` or `video_sender` must not be nullptr,
  // and must either outlive `this` or live until `Stop` is called. If
  // either is nullptr, the associated config should be default constructed as
  // it is ignored.
  void StartRpcMessaging(
      scoped_refptr<media::cast::CastEnvironment> cast_environment,
      std::unique_ptr<openscreen::cast::Sender> audio_sender,
      std::unique_ptr<openscreen::cast::Sender> video_sender,
      std::optional<media::cast::FrameSenderConfig> audio_config,
      std::optional<media::cast::FrameSenderConfig> video_config);

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
  // Called by the public |StartRpcMessaging| methods.
  void StartRpcMessagingInternal(
      scoped_refptr<media::cast::CastEnvironment> cast_environment,
      std::optional<media::cast::FrameSenderConfig> audio_config,
      std::optional<media::cast::FrameSenderConfig> video_config);

  // Called by RemotingSender when error occurred. Will stop this remoting
  // session and fallback to mirroring.
  void OnRemotingDataStreamError();

  raw_ref<Client> client_;
  const media::mojom::RemotingSinkMetadata sink_metadata_;
  raw_ref<RpcDispatcher> rpc_dispatcher_;
  mojo::Receiver<media::mojom::Remoter> receiver_{this};
  mojo::Remote<media::mojom::RemotingSource> remoting_source_;
  scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  std::unique_ptr<RemotingSender> audio_sender_;
  std::unique_ptr<RemotingSender> video_sender_;

  // Used only if StartRpcMessaging is called with openscreen::cast::Sender
  // objects.
  std::unique_ptr<openscreen::cast::Sender> openscreen_audio_sender_;
  std::unique_ptr<openscreen::cast::Sender> openscreen_video_sender_;

  std::optional<media::cast::FrameSenderConfig> audio_config_;
  std::optional<media::cast::FrameSenderConfig> video_config_;

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
