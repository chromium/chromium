// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_SESSION_HOST_H_
#define COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_SESSION_HOST_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/resource_provider.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "components/mirroring/service/media_remoter.h"
#include "components/mirroring/service/mirror_settings.h"
#include "components/mirroring/service/mirroring_logger.h"
#include "components/mirroring/service/openscreen_message_port.h"
#include "components/mirroring/service/openscreen_stats_client.h"
#include "components/mirroring/service/rpc_dispatcher.h"
#include "components/mirroring/service/rtp_stream.h"
#include "components/openscreen_platform/event_trace_logging_platform.h"
#include "components/openscreen_platform/task_runner.h"
#include "gpu/config/gpu_info.h"
#include "media/capture/video/video_capture_feedback.h"
#include "media/cast/cast_environment.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/openscreen/src/cast/streaming/public/sender_session.h"

using openscreen::cast::capture_recommendations::Recommendations;

namespace base {
class OneShotTimer;
}

namespace media {
class AudioInputDevice;

}  // namespace media

namespace viz {
class Gpu;
}  // namespace viz

namespace mirroring {

class RpcDispatcher;
class VideoCaptureClient;

// Minimum required bitrate used for calculating bandwidth.
constexpr int kMinRequiredBitrate = 384 << 10;  // 384 kbps

// Default bitrate used before we have a calculation.
constexpr int kDefaultBitrate = kMinRequiredBitrate * 2;  // 768 kbps

// Hosts a streaming session by hosting an `openscreen::cast::SenderSession` and
// doing all of the necessary interfacing for audio and video capture, switching
// between mirroring and remoting, and setting up audio and video streams to
// encode and send captured content.
//
// On construction, an Open Screen SenderSession is immediately created and
// negotiation of a streaming session is started. The session host will stay
// in a good state until either the mirroring service notices a disconnection
// and tears down this streaming session, or a fatal error occurs.
//
// NOTE: most methods should be called on the same sequence as construction.
// This class also uses additional task runners, such as the IO task runner of
// this utility process for accessing the GPU, and dedicated video and audio
// encoder threads. Finally, some methods such as
// AudioCapturingCallback::Capture may be called on the audio thread.
class COMPONENT_EXPORT(MIRRORING_SERVICE) OpenscreenSessionHost final
    : public RtpStreamClient,
      public openscreen::cast::SenderSession::Client,
      public MediaRemoter::Client {
 public:
  // NOTE: some notes on constructor arguments:
  //    `session_params`: connection information for the receiver.
  //    `max_resolution`: width and height that should never be exceeded.
  // `resource_provider`: interface to ask the browser for resources.
  //  `outbound_channel`: used to send cast messages to the receiver.
  //   `inbound_channel`: used to receiver cast messages from the receiver.
  //    `io_task_runner`: used to interact with the GPU through Viz. This arg
  //                      must be passed to enable hardware encoding.
  OpenscreenSessionHost(
      mojom::SessionParametersPtr session_params,
      const gfx::Size& max_resolution,
      mojo::PendingRemote<mojom::SessionObserver> observer,
      mojo::PendingRemote<mojom::ResourceProvider> resource_provider,
      mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
      mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ~OpenscreenSessionHost() override;

  // Initializes some of the asynchronous components of the session host, such
  // as access to the GPU. Must be called before negotiation of a session
  // can begin.
  using AsyncInitializedCallback = base::OnceCallback<void()>;
  void AsyncInitialize(AsyncInitializedCallback done_cb = {});

  // SenderSession::Client overrides.
  void OnNegotiated(const openscreen::cast::SenderSession* session,
                    openscreen::cast::SenderSession::ConfiguredSenders senders,
                    Recommendations capture_recommendations) override;
  void OnCapabilitiesDetermined(
      const openscreen::cast::SenderSession* session,
      openscreen::cast::RemotingCapabilities capabilities) override;
  void OnError(const openscreen::cast::SenderSession* session,
               const openscreen::Error& error) override;

  // RtpStreamClient overrides.
  void OnError(const std::string& message) override;
  void RequestRefreshFrame() override;
  void CreateVideoEncodeAccelerator(
      media::cast::ReceiveVideoEncodeAcceleratorCallback callback) override;

  // MediaRemoter::Client overrides.
  void ConnectToRemotingSource(
      mojo::PendingRemote<media::mojom::Remoter> remoter,
      mojo::PendingReceiver<media::mojom::RemotingSource> source_receiver)
      override;
  void RequestRemotingStreaming() override;
  void RestartMirroringStreaming() override;

  void SwitchSourceTab();

  // Callback by media::cast::VideoSender to set a new target playout delay.
  void SetTargetPlayoutDelay(base::TimeDelta playout_delay);

  base::Value::Dict GetMirroringStats() const;
  void SetSenderStatsForTest(const openscreen::cast::SenderStats& test_stats);

 private:
  friend class OpenscreenSessionHostTest;
  FRIEND_TEST_ALL_PREFIXES(OpenscreenSessionHostTest, ChangeTargetPlayoutDelay);
  FRIEND_TEST_ALL_PREFIXES(OpenscreenSessionHostTest, UpdateBandwidthEstimate);
  class AudioCapturingCallback;
  using SupportedProfiles = media::VideoEncodeAccelerator::SupportedProfiles;

  // Called when the GPU is either set up or determined to be unavailable due
  // to software rendering being used.
  void OnAsyncInitialized(const SupportedProfiles& profiles);

  // Notify `observer_` that error occurred and close the session.
  void ReportAndLogError(mojom::SessionError error, std::string_view message);

  // Stops the current streaming session. If not called from StopSession(), a
  // new streaming session will start later after exchanging OFFER/ANSWER
  // messages with the receiver. This could happen any number of times before
  // StopSession() shuts down everything permanently.
  void StopStreaming();

  // Shuts down the entire mirroring session.
  void StopSession();

  // Helper method for taking the recommendations given by the Open Screen
  // library and applying them to the given audio and video configs.
  void SetConstraints(
      const Recommendations& recommendations,
      std::optional<media::cast::FrameSenderConfig>& audio_config,
      std::optional<media::cast::FrameSenderConfig>& video_config);

  // Sends a request to create an audio input stream through the Audio Service,
  // configured with the specified audio `params`. The `shared_memory_count`
  // property indicates how many equal-lengthed segments exist in the shared
  // memory buffer. Once the stream has been created, `client` is called.
  void CreateAudioStream(
      mojo::PendingRemote<mojom::AudioStreamCreatorClient> client,
      const media::AudioParameters& params,
      uint32_t shared_memory_count);

  // Callback by Audio/VideoSender to indicate encoder status change.
  void OnEncoderStatusChange(const media::cast::FrameSenderConfig& config,
                             media::cast::OperationalStatus status);

  // Callback by media::cast::VideoSender to report resource utilization.
  void ProcessFeedback(const media::VideoCaptureFeedback& feedback);

  // Called by OpenscreenFrameSender to determine bitrate.
  int GetSuggestedVideoBitrate(int min_bitrate, int max_bitrate) const;

  // Called periodically to update the `bandwidth_estimate_`.
  void UpdateBandwidthEstimate();

  // Create and send OFFER message.
  void Negotiate();
  void NegotiateMirroring();
  void NegotiateRemoting();

  // Initialize `media_remoter_` and `rpc_dispatcher_`.
  void InitMediaRemoter(
      const openscreen::cast::RemotingCapabilities& capabilities);

  // Called 5 seconds after the `media_remoter_` is initialized for Remote
  // Playabck sessions. It terminates the streaming session if remoting is not
  // started when it's called.
  void OnRemotingStartTimeout();

  // Called to provide Open Screen with access to this host's network proxy.
  network::mojom::NetworkContext* GetNetworkContext();

  // Provided by client.
  const mojom::SessionParameters session_params_;

  // State transition:
  // kInitializing
  //     |
  //     ↓
  // kMirroring <-------> kRemoting
  //     |                   |
  //     `---> kStopped <----'
  //
  // NOTE: once a session has reached a kStopped state, it cannot be
  // reinitialized or used.
  enum class State {
    // The session is initializing, and can't be used yet.
    kInitializing,

    // A mirroring streaming session is starting or started.
    kMirroring,

    // A remoting streaming session is starting or started.
    kRemoting,

    // The session is stopped due to a user request or a fatal error.
    kStopped,
  };
  State state_ = State::kInitializing;

  // Informed of changes to session state.
  mojo::Remote<mojom::SessionObserver> observer_;

  // Provides a variety of instances, such as the current network context.
  mojo::Remote<mojom::ResourceProvider> resource_provider_;

  // Implements an Open Screen message port and wraps inbound and outbound mojom
  // channels.
  OpenscreenMessagePort message_port_;

  // Utility object for logging.
  MirroringLogger logger_;

  // Used to initialize video and audio capture clients.
  MirrorSettings mirror_settings_;

  // Used to wrap the current thread's sequenced task runner for use by Open
  // Screen.
  std::unique_ptr<openscreen_platform::TaskRunner> openscreen_task_runner_;

  // Used to wrap the `openscreen_task_runner` as well as the clock and
  // local endpoint for binding. Responsible for creating and binding a UDP
  // socket.
  std::unique_ptr<openscreen::cast::Environment> openscreen_environment_;

  // Takes care of handling OFFER/ANSWER negotiations, as well as querying
  // capabilities and creating openscreen::cast::Sender objects upon
  // negotiation.
  std::unique_ptr<openscreen::cast::SenderSession> session_;

  // Used to provide access to UDP sockets and URL loading.
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  bool set_network_context_proxy_ = false;

  // Stored as part of generating an OFFER.
  // NOTE: currently we only support Opus audio, but may provide a variety of
  // video codec configurations.
  std::optional<media::cast::FrameSenderConfig> last_offered_audio_config_;
  std::vector<media::cast::FrameSenderConfig> last_offered_video_configs_;

  // Created after OFFER/ANSWER exchange succeeds.
  std::unique_ptr<AudioRtpStream> audio_stream_;
  std::unique_ptr<VideoRtpStream> video_stream_;

  // Connects to the video capture host and launches the video capture device.
  std::unique_ptr<VideoCaptureClient> video_capture_client_;

  // Manages the clock and thread proxies for the audio sender, video sender,
  // and media remoter.
  scoped_refptr<media::cast::CastEnvironment> cast_environment_;

  // Task runners used specifically for audio, video encoding.
  scoped_refptr<base::SingleThreadTaskRunner> audio_encode_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> video_encode_thread_;

  // Called when audio is successfully captured by `audio_input_device_`.
  std::unique_ptr<AudioCapturingCallback> audio_capturing_callback_;

  // Captures audio samples from the resourceprovider-created audio stream.
  scoped_refptr<media::AudioInputDevice> audio_input_device_;

  // Used as an interface for the media remoter to send RPC messages. Created
  // when a successful capabilities response arrives.
  std::unique_ptr<RpcDispatcher> rpc_dispatcher_;

  // Manages remoting content to the Cast Receiver. Created when a successful
  // capabilities response arrives.
  std::unique_ptr<MediaRemoter> media_remoter_;

  // GPU specific properties, used to indicate whether HW encoding should be
  // used and to help initialize it if enabled.
  std::unique_ptr<viz::Gpu> gpu_;
  SupportedProfiles supported_profiles_;
  mojo::Remote<media::mojom::VideoEncodeAcceleratorProvider> vea_provider_;

  // Called when the session host has fully initialized.
  AsyncInitializedCallback initialized_cb_;

  // Used to periodically update the currently used bandwidth estimate.
  base::RepeatingTimer bandwidth_update_timer_;

  // Used to override getting the bandwidth from the session. Setting to a
  // positive value causes the session's bandwidth estimation to not be called.
  int forced_bandwidth_estimate_for_testing_ = 0;

  // The portion of the bandwidth estimate that is currently available for use.
  // Note that the actual bandwidth will be effectively capped at the sum of the
  // current video and audio bitrates.
  int usable_bandwidth_ = kDefaultBitrate;

  // Indicate whether we're in the middle of switching tab sources.
  bool switching_tab_source_ = false;
  // This timer is used to stop the session in case Remoting is not started
  // before timeout. The timer is stopped when Remoting session successfully
  // starts.
  base::OneShotTimer remote_playback_start_timer_;
  // Records the time when the streaming session is started and `media_remoter_`
  // is initialized.
  std::optional<base::Time> remote_playback_start_time_;

  // An optional stats client for fetching quality statistics from an Openscreen
  // casting session.
  std::unique_ptr<OpenscreenStatsClient> stats_client_;

  // Used in callbacks executed on task runners, such as by RtpStream.
  // TODO(crbug.com/40238714): determine if weak pointers can be removed.
  base::WeakPtrFactory<OpenscreenSessionHost> weak_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_SESSION_HOST_H_
