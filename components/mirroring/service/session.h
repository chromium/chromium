// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_SESSION_H_
#define COMPONENTS_MIRRORING_SERVICE_SESSION_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/resource_provider.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "components/mirroring/service/media_remoter.h"
#include "components/mirroring/service/message_dispatcher.h"
#include "components/mirroring/service/mirror_settings.h"
#include "components/mirroring/service/rpc_dispatcher_impl.h"
#include "components/mirroring/service/rtp_stream.h"
#include "components/mirroring/service/session_logger.h"
#include "gpu/config/gpu_info.h"
#include "media/capture/video/video_capture_feedback.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class OneShotTimer;
}
namespace media {
class AudioInputDevice;
namespace cast {
class CastTransport;
}  // namespace cast
}  // namespace media

namespace viz {
class Gpu;
}  // namespace viz

namespace mirroring {

class ReceiverResponse;
class VideoCaptureClient;

// Controls a mirroring session, including audio/video capturing, Cast
// Streaming, and the switching to/from media remoting. When constructed, it
// does OFFER/ANSWER exchange with the mirroring receiver. Mirroring starts when
// the exchange succeeds and stops when this class is destructed or error
// occurs. Specifically, a session is torn down when (1) a new session starts,
// (2) the mirroring service note a disconnection.
// |observer| will get notified when status changes. |outbound_channel| is
// responsible for sending messages to the mirroring receiver through Cast
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

  using AsyncInitializeDoneCB = base::OnceCallback<void()>;
  void AsyncInitialize(AsyncInitializeDoneCB done_cb);

  // RtpStreamClient implementation.
  void OnError(const std::string& message) override;
  void RequestRefreshFrame() override;
  void CreateVideoEncodeAccelerator(
      media::cast::ReceiveVideoEncodeAcceleratorCallback callback) override;

  // Callbacks by media::cast::CastTransport::Client.
  void OnTransportStatusChanged(media::cast::CastTransportStatus status);
  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<media::cast::FrameEvent>> frame_events,
      std::unique_ptr<std::vector<media::cast::PacketEvent>> packet_events);

  // Helper method for applying the constraints from |answer| to the audio and
  // video configs.
  void ApplyConstraintsToConfigs(
      const openscreen::cast::Answer& answer,
      absl::optional<media::cast::FrameSenderConfig>& audio_config,
      absl::optional<media::cast::FrameSenderConfig>& video_config);

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

  void SwitchSourceTab();

 private:
  class AudioCapturingCallback;
  using SupportedProfiles = media::VideoEncodeAccelerator::SupportedProfiles;

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

  // Send logging messages to |observer_|.
  void LogInfoMessage(const std::string& message);
  void LogErrorMessage(const std::string& message);

  // Callback by Audio/VideoSender to indicate encoder status change.
  void OnEncoderStatusChange(media::cast::OperationalStatus status);

  // Callback by media::cast::VideoSender to set a new target playout delay.
  void SetTargetPlayoutDelay(base::TimeDelta playout_delay);

  // Callback by media::cast::VideoSender to report resource utilization.
  void ProcessFeedback(const media::VideoCaptureFeedback& feedback);

  // Create and send OFFER message.
  void CreateAndSendOffer();

  // Send GET_CAPABILITIES message.
  void QueryCapabilitiesForRemoting();

  // Initialize `media_remoter_` and `rpc_dispatcher_`.
  void InitMediaRemoter(const std::vector<std::string>& caps);
  // Called 5 seconds after the `media_remoter_` is initialized for Remote
  // Playabck sessions. It terminates the streaming session if remoting is not
  // started when it's called.
  void OnRemotingStartTimeout();

  void OnAsyncInitializeDone(const SupportedProfiles& profiles);

  // Provided by client.
  const mojom::SessionParameters session_params_;

  // State transition:
  // INITIALIZING
  //     |
  //    \./
  // MIRRORING <-------> REMOTING
  //     |                   |
  //     .---> STOPPED <----.
  enum {
    INITIALIZING,  // The session is initializing, and can't be used yet.
    MIRRORING,     // A mirroring streaming session is starting or started.
    REMOTING,      // A remoting streaming session is starting or started.
    STOPPED,       // The session is stopped due to user's request or errors.
  } state_ = INITIALIZING;

  mojo::Remote<mojom::SessionObserver> observer_;
  mojo::Remote<mojom::ResourceProvider> resource_provider_;
  MirrorSettings mirror_settings_;

  std::unique_ptr<MessageDispatcher> message_dispatcher_;
  std::unique_ptr<RpcDispatcherImpl> rpc_dispatcher_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;

  // Created after OFFER/ANSWER exchange succeeds.
  std::unique_ptr<AudioRtpStream> audio_stream_;
  std::unique_ptr<VideoRtpStream> video_stream_;
  std::unique_ptr<VideoCaptureClient> video_capture_client_;
  scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  std::unique_ptr<media::cast::CastTransport> cast_transport_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_encode_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> video_encode_thread_;
  std::unique_ptr<AudioCapturingCallback> audio_capturing_callback_;
  scoped_refptr<media::AudioInputDevice> audio_input_device_;
  std::unique_ptr<MediaRemoter> media_remoter_;
  std::unique_ptr<viz::Gpu> gpu_;
  SupportedProfiles supported_profiles_;
  mojo::Remote<media::mojom::VideoEncodeAcceleratorProvider> vea_provider_;
  std::unique_ptr<SessionLogger> session_logger_;

  // A callback to call after initialization is completed
  AsyncInitializeDoneCB init_done_cb_;

  // Indicates whether we're in the middle of switching tab sources.
  bool switching_tab_source_ = false;

  // This timer is used to stop the session in case Remoting is not started
  // before timeout. The timer is stopped when Remoting session successfully
  // starts.
  base::OneShotTimer remote_playback_start_timer_;
  // Records the time when the streaming session is started and `media_remoter_`
  // is initialized.
  absl::optional<base::Time> remote_playback_start_time_;

  base::WeakPtrFactory<Session> weak_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_SESSION_H_
