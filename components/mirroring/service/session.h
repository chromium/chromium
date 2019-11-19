// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_SESSION_H_
#define COMPONENTS_MIRRORING_SERVICE_SESSION_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/resource_provider.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "components/mirroring/service/media_remoter.h"
#include "components/mirroring/service/message_dispatcher.h"
#include "components/mirroring/service/mirror_settings.h"
#include "components/mirroring/service/rtp_stream.h"
#include "components/mirroring/service/session_monitor.h"
#include "components/mirroring/service/wifi_status_monitor.h"
#include "gpu/config/gpu_info.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace media {
class AudioInputDevice;
namespace cast {
class CastTransport;
}  // namespace cast
}  // namespace media

namespace gpu {
class GpuChannelHost;
}  // namespace gpu

namespace viz {
class Gpu;
}  // namespace viz

namespace mirroring {

struct ReceiverResponse;
class VideoCaptureClient;
class SessionMonitor;

// Controls a mirroring session, including audio/video capturing, Cast
// Streaming, and the switching to/from media remoting. When constructed, it
// does OFFER/ANSWER exchange with the mirroring receiver. Mirroring starts when
// the exchange succeeds and stops when this class is destructed or error
// occurs. |observer| will get notified when status changes. |outbound_channel|
// is responsible for sending messages to the mirroring receiver through Cast
// Channel. |inbound_channel| receives message sent from the mirroring receiver.
class COMPONENT_EXPORT(MIRRORING_SERVICE) Session final
    : public RtpStreamClient,
      public MediaRemoter::Client {
 public:
  Session(mojom::SessionParametersPtr session_params,
          const gfx::Size& max_resolution,
          mojo::PendingRemote<mojom::SessionObserver> observer,
          mojo::PendingRemote<mojom::ResourceProvider> resource_provider,
          mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
          mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel,
          scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ~Session() override;

  // RtpStreamClient implemenation.
  void OnError(const std::string& message) override;
  void RequestRefreshFrame() override;
  void CreateVideoEncodeAccelerator(
      const media::cast::ReceiveVideoEncodeAcceleratorCallback& callback)
      override;
  void CreateVideoEncodeMemory(
      size_t size,
      const media::cast::ReceiveVideoEncodeMemoryCallback& callback) override;

  // Callbacks by media::cast::CastTransport::Client.
  void OnTransportStatusChanged(media::cast::CastTransportStatus status);
  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<media::cast::FrameEvent>> frame_events,
      std::unique_ptr<std::vector<media::cast::PacketEvent>> packet_events);

  // Callback for ANSWER response. If the ANSWER is invalid, |observer_| will
  // get notified with error, and session is stopped. Otherwise, capturing and
  // streaming are started with the selected configs.
  void OnAnswer(
      const std::vector<media::cast::FrameSenderConfig>& audio_configs,
      const std::vector<media::cast::FrameSenderConfig>& video_configs,
      const ReceiverResponse& response);

  // Called by |message_dispatcher_| when error occurs while parsing the
  // responses.
  void OnResponseParsingError(const std::string& error_message);

  // Creates an audio input stream through Audio Service. |client| will be
  // called after the stream is created.
  void CreateAudioStream(
      mojo::PendingRemote<mojom::AudioStreamCreatorClient> client,
      const media::AudioParameters& params,
      uint32_t shared_memory_count);

  // Callback for CAPABILITIES_RESPONSE.
  void OnCapabilitiesResponse(const ReceiverResponse& response);

 private:
  class AudioCapturingCallback;

  // MediaRemoter::Client implementation.
  void ConnectToRemotingSource(
      mojo::PendingRemote<media::mojom::Remoter> remoter,
      mojo::PendingReceiver<media::mojom::RemotingSource> source_receiver)
      override;
  void RequestRemotingStreaming() override;
  void RestartMirroringStreaming() override;

  // Stops the current streaming session. If not called from StopSession(), a
  // new streaming session will start later after exchanging OFFER/ANSWER
  // messages with the receiver. This could happen any number of times before
  // StopSession() shuts down everything permanently.
  void StopStreaming();

  void StopSession();  // Shuts down the entire mirroring session.

  // Notify |observer_| that error occurred and close the session.
  void ReportError(mojom::SessionError error);

  // Callback by Audio/VideoSender to indicate encoder status change.
  void OnEncoderStatusChange(media::cast::OperationalStatus status);

  // Callback by media::cast::VideoSender to set a new target playout delay.
  void SetTargetPlayoutDelay(base::TimeDelta playout_delay);

  media::VideoEncodeAccelerator::SupportedProfiles GetSupportedVeaProfiles();

  // Create and send OFFER message.
  void CreateAndSendOffer();

  // Send GET_CAPABILITIES message.
  void QueryCapabilitiesForRemoting();

  // Provided by client.
  const mojom::SessionParameters session_params_;

  // State transition:
  // MIRRORING <-------> REMOTING
  //     |                   |
  //     .---> STOPPED <----.
  enum {
    MIRRORING,  // A mirroring streaming session is starting or started.
    REMOTING,   // A remoting streaming session is starting or started.
    STOPPED,    // The session is stopped due to user's request or errors.
  } state_;

  mojo::Remote<mojom::SessionObserver> observer_;
  mojo::Remote<mojom::ResourceProvider> resource_provider_;
  MirrorSettings mirror_settings_;

  MessageDispatcher message_dispatcher_;

  mojo::Remote<network::mojom::NetworkContext> network_context_;

  base::Optional<SessionMonitor> session_monitor_;

  // Created after OFFER/ANSWER exchange succeeds.
  std::unique_ptr<AudioRtpStream> audio_stream_;
  std::unique_ptr<VideoRtpStream> video_stream_;
  std::unique_ptr<VideoCaptureClient> video_capture_client_;
  scoped_refptr<media::cast::CastEnvironment> cast_environment_ = nullptr;
  std::unique_ptr<media::cast::CastTransport> cast_transport_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_encode_thread_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> video_encode_thread_ = nullptr;
  std::unique_ptr<AudioCapturingCallback> audio_capturing_callback_;
  scoped_refptr<media::AudioInputDevice> audio_input_device_;
  std::unique_ptr<MediaRemoter> media_remoter_;
  std::unique_ptr<viz::Gpu> gpu_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;
  gpu::VideoEncodeAcceleratorSupportedProfiles supported_profiles_;
  media::mojom::VideoEncodeAcceleratorProviderPtr vea_provider_;

  base::WeakPtrFactory<Session> weak_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_SESSION_H_
