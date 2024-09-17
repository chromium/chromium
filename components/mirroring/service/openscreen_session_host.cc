// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/openscreen_session_host.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/cpu.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/mirroring/service/captured_audio_input.h"
#include "components/mirroring/service/mirroring_features.h"
#include "components/mirroring/service/rpc_dispatcher_impl.h"
#include "components/mirroring/service/video_capture_client.h"
#include "components/openscreen_platform/network_context.h"
#include "components/openscreen_platform/network_util.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/audio/audio_input_device.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/video_codecs.h"
#include "media/capture/video_capture_types.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/packet.h"
#include "media/cast/encoding/encoding_support.h"
#include "media/cast/openscreen/config_conversions.h"
#include "media/cast/sender/audio_sender.h"
#include "media/cast/sender/video_sender.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/mojo/clients/mojo_video_encode_accelerator.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/ip_endpoint.h"
#include "services/viz/public/cpp/gpu/gpu.h"
#include "third_party/openscreen/src/cast/streaming/public/answer_messages.h"
#include "third_party/openscreen/src/cast/streaming/public/capture_recommendations.h"
#include "third_party/openscreen/src/cast/streaming/public/environment.h"
#include "third_party/openscreen/src/cast/streaming/public/offer_messages.h"

using media::cast::FrameEvent;
using media::cast::FrameSenderConfig;
using media::cast::OperationalStatus;
using media::cast::Packet;
using media::cast::PacketEvent;
using media::cast::RtpPayloadType;
using media::mojom::RemotingSinkAudioCapability;
using media::mojom::RemotingSinkVideoCapability;
using mirroring::mojom::SessionError;
using mirroring::mojom::SessionType;

namespace mirroring {
namespace {

// The time between updating the bandwidth estimates.
constexpr base::TimeDelta kBandwidthUpdateInterval = base::Milliseconds(500);

// The maximum time that Session will wait for Remoter to start Remoting. If
// timeout occurs, the session is terminated.
constexpr base::TimeDelta kStartRemotePlaybackTimeOut = base::Seconds(5);

constexpr char kLogPrefix[] = "OpenscreenSessionHost";

int NumberOfEncodeThreads() {
  // Do not saturate CPU utilization just for encoding. On a lower-end system
  // with only 1 or 2 cores, use only one thread for encoding. On systems with
  // more cores, allow half of the cores to be used for encoding.
  return std::min(8, (base::SysInfo::NumberOfProcessors() + 1) / 2);
}

// Convert the sink capabilities to media::mojom::RemotingSinkMetadata.
media::mojom::RemotingSinkMetadata ToRemotingSinkMetadata(
    const openscreen::cast::RemotingCapabilities& capabilities,
    const std::string& friendly_name) {
  media::mojom::RemotingSinkMetadata sink_metadata;
  sink_metadata.friendly_name = friendly_name;

  for (const openscreen::cast::AudioCapability capability :
       capabilities.audio) {
    switch (capability) {
      case openscreen::cast::AudioCapability::kBaselineSet:
        sink_metadata.audio_capabilities.push_back(
            RemotingSinkAudioCapability::CODEC_BASELINE_SET);
        continue;

      case openscreen::cast::AudioCapability::kAac:
        sink_metadata.audio_capabilities.push_back(
            RemotingSinkAudioCapability::CODEC_AAC);
        continue;

      case openscreen::cast::AudioCapability::kOpus:
        sink_metadata.audio_capabilities.push_back(
            RemotingSinkAudioCapability::CODEC_OPUS);
        continue;
    }
    NOTREACHED_IN_MIGRATION();
  }

  for (const openscreen::cast::VideoCapability capability :
       capabilities.video) {
    switch (capability) {
      case openscreen::cast::VideoCapability::kSupports4k:
        sink_metadata.video_capabilities.push_back(
            RemotingSinkVideoCapability::SUPPORT_4K);
        continue;

      case openscreen::cast::VideoCapability::kH264:
        sink_metadata.video_capabilities.push_back(
            RemotingSinkVideoCapability::CODEC_H264);
        continue;

      case openscreen::cast::VideoCapability::kVp8:
        sink_metadata.video_capabilities.push_back(
            RemotingSinkVideoCapability::CODEC_VP8);
        continue;

      case openscreen::cast::VideoCapability::kVp9:
        sink_metadata.video_capabilities.push_back(
            RemotingSinkVideoCapability::CODEC_VP9);
        continue;

      case openscreen::cast::VideoCapability::kHevc:
        sink_metadata.video_capabilities.push_back(
            RemotingSinkVideoCapability::CODEC_HEVC);
        continue;

      // TODO(crbug.com/40238534): remoting should support AV1.
      case openscreen::cast::VideoCapability::kAv1:
        continue;
    }
    NOTREACHED_IN_MIGRATION();
  }

  return sink_metadata;
}

void UpdateConfigUsingSessionParameters(
    const mojom::SessionParameters& session_params,
    FrameSenderConfig& config) {
  if (session_params.target_playout_delay) {
    // TODO(crbug.com/40238532): adaptive playout delay should be
    // re-enabled.
    config.min_playout_delay = *session_params.target_playout_delay;
    config.max_playout_delay = *session_params.target_playout_delay;
  }
}

void UpdateAudioConfigMaxBitrate(FrameSenderConfig& audio_config) {
  CHECK(audio_config.is_audio());

  // Taken from the legacy Session implementation.
  // TODO(https://crbug.com/1316434): this matches legacy behavior, but
  // testing should be done as part of migration to this class to determine
  // what the right long term behavior is.
  //
  // Note on "AUTO" bitrate calculation: This is based on libopus source
  // at the time of this writing. Internally, it uses the following math:
  //
  //   packet_overhead_bps = 60 bits * num_packets_in_one_second
  //   approx_encoded_signal_bps = frequency * channels
  //   estimated_bps = packet_overhead_bps + approx_encoded_signal_bps
  //
  // For 100 packets/sec at 48 kHz and 2 channels, this is 102kbps.
  if (audio_config.max_bitrate == 0) {
    audio_config.max_bitrate =
        (60 * audio_config.max_frame_rate +
         audio_config.rtp_timebase * audio_config.channels);
  }
}

const std::string ToString(const media::VideoCaptureParams& params) {
  return base::StringPrintf(
      "requested_format = %s, buffer_type = %d, resolution_policy = %d",
      media::VideoCaptureFormat::ToString(params.requested_format).c_str(),
      static_cast<int>(params.buffer_type),
      static_cast<int>(params.resolution_change_policy));
}

void RecordRemotePlaybackSessionLoadTime(std::optional<base::Time> start_time) {
  if (!start_time) {
    return;
  }
  base::TimeDelta time_delta = base::Time::Now() - start_time.value();
  base::UmaHistogramTimes("MediaRouter.RemotePlayback.SessionLoadTime",
                          time_delta);
}

void RecordRemotePlaybackSessionStartsBeforeTimeout(bool started) {
  base::UmaHistogramBoolean(
      "MediaRouter.RemotePlayback.SessionStartsBeforeTimeout", started);
}

// Returns a message that can be reported alongside an error status. If not a
// reportable error, returns nullptr.
const char* AsErrorMessage(OperationalStatus status) {
  switch (status) {
    case OperationalStatus::STATUS_UNINITIALIZED:
    case OperationalStatus::STATUS_CODEC_REINIT_PENDING:
    // Not an error.
    // TODO(crbug.com/40103719): As an optimization, signal the client to pause
    // sending more frames until the state becomes STATUS_INITIALIZED again.
    case OperationalStatus::STATUS_INITIALIZED:
      return nullptr;

    case OperationalStatus::STATUS_INVALID_CONFIGURATION:
      return "Invalid encoder configuration.";

    case OperationalStatus::STATUS_UNSUPPORTED_CODEC:
      return "Unsupported codec.";

    case OperationalStatus::STATUS_CODEC_INIT_FAILED:
      return "Failed to initialize codec.";

    case OperationalStatus::STATUS_CODEC_RUNTIME_ERROR:
      return "Fatal error in codec runtime.";
  }
}
}  // namespace

// Receives data from the audio capturer source, and calls `audio_data_callback`
// when new data is available. If `error_callback_` is called, the consumer
// should tear down this instance.
class OpenscreenSessionHost::AudioCapturingCallback final
    : public media::AudioCapturerSource::CaptureCallback {
 public:
  using AudioDataCallback =
      base::RepeatingCallback<void(std::unique_ptr<media::AudioBus> audio_bus,
                                   base::TimeTicks recorded_time)>;
  using ErrorCallback = base::OnceCallback<void(std::string_view)>;
  AudioCapturingCallback(AudioDataCallback audio_data_callback,
                         ErrorCallback error_callback,
                         mojo::Remote<mojom::SessionObserver>& observer)
      : audio_data_callback_(std::move(audio_data_callback)),
        error_callback_(std::move(error_callback)),
        logger_("AudioCapturingCallback", observer) {
    DCHECK(!audio_data_callback_.is_null());
  }

  AudioCapturingCallback(const AudioCapturingCallback&) = delete;
  AudioCapturingCallback& operator=(const AudioCapturingCallback&) = delete;

  ~AudioCapturingCallback() override = default;

 private:
  // media::AudioCapturerSource::CaptureCallback implementation.
  void OnCaptureStarted() override { logger_.LogInfo("OnCaptureStarted"); }

  // Called on audio thread.
  void Capture(const media::AudioBus* audio_bus,
               base::TimeTicks audio_capture_time,
               const media::AudioGlitchInfo& glitch_info,
               double volume,
               bool key_pressed) override {
    if (!has_captured_) {
      logger_.LogInfo(
          base::StringPrintf("first Capture(): volume = %f", volume));
      has_captured_ = true;
    }
    // TODO(crbug.com/40103719): Don't copy the audio data. Instead, send
    // |audio_bus| directly to the encoder.
    std::unique_ptr<media::AudioBus> captured_audio =
        media::AudioBus::Create(audio_bus->channels(), audio_bus->frames());
    audio_bus->CopyTo(captured_audio.get());
    audio_data_callback_.Run(std::move(captured_audio), audio_capture_time);
  }

  void OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                      const std::string& message) override {
    std::string error_message = base::StrCat(
        {"AudioCaptureError occurred, code: ",
         base::NumberToString(static_cast<int>(code)), ", message: ", message});
    if (!error_callback_.is_null())
      std::move(error_callback_).Run(error_message);
  }

  void OnCaptureMuted(bool is_muted) override {
    logger_.LogInfo(base::StrCat(
        {"OnCaptureMuted, is_muted = ", base::NumberToString(is_muted)}));
  }

  const AudioDataCallback audio_data_callback_;
  ErrorCallback error_callback_;
  MirroringLogger logger_;
  bool has_captured_ = false;
};

OpenscreenSessionHost::OpenscreenSessionHost(
    mojom::SessionParametersPtr session_params,
    const gfx::Size& max_resolution,
    mojo::PendingRemote<mojom::SessionObserver> observer,
    mojo::PendingRemote<mojom::ResourceProvider> resource_provider,
    mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
    mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : session_params_(*session_params),
      observer_(std::move(observer)),
      resource_provider_(std::move(resource_provider)),
      message_port_(session_params_.source_id,
                    session_params_.destination_id,
                    std::move(outbound_channel),
                    std::move(inbound_channel)),
      logger_(kLogPrefix, observer_) {
  DCHECK(resource_provider_);

  openscreen_platform::EventTraceLoggingPlatform::EnsureInstance();

  mirror_settings_.SetResolutionConstraints(max_resolution.width(),
                                            max_resolution.height());
  resource_provider_->GetNetworkContext(
      network_context_.BindNewPipeAndPassReceiver());

  // Access to the network context for Open Screen components is granted only
  // by our `resource_provider_`'s NetworkContext mojo interface.
  if (!openscreen_platform::HasNetworkContextGetter()) {
    set_network_context_proxy_ = true;

    // NOTE: use of `base::Unretained` is safe since we clear the getter on
    // destruction.
    openscreen_platform::SetNetworkContextGetter(base::BindRepeating(
        &OpenscreenSessionHost::GetNetworkContext, base::Unretained(this)));
  }

  // In order to access the mojo Network interface, all of the networking
  // related Open Screen tasks must be ran on the same sequence to avoid
  // checking errors.
  openscreen_task_runner_ = std::make_unique<openscreen_platform::TaskRunner>(
      base::SequencedTaskRunner::GetCurrentDefault());

  // The Open Screen environment should not be set up until after the network
  // context is set up.
  openscreen_environment_ = std::make_unique<openscreen::cast::Environment>(
      openscreen::Clock::now, *openscreen_task_runner_,
      openscreen::IPEndpoint::kAnyV4());

  if (session_params->type != mojom::SessionType::AUDIO_ONLY &&
      io_task_runner) {
    mojo::PendingRemote<viz::mojom::Gpu> remote_gpu;
    resource_provider_->BindGpu(remote_gpu.InitWithNewPipeAndPassReceiver());
    gpu_ = viz::Gpu::Create(std::move(remote_gpu), io_task_runner);
  }

  session_ = std::make_unique<openscreen::cast::SenderSession>(
      openscreen::cast::SenderSession::Configuration{
          .remote_address = media::cast::ToOpenscreenIPAddress(
              session_params_.receiver_address),
          .client = *this,
          .environment = openscreen_environment_.get(),
          .message_port = &message_port_,
          .message_source_id = session_params_.source_id,
          .message_destination_id = session_params_.destination_id});

  if (session_params_.enable_rtcp_reporting) {
    stats_client_ = std::make_unique<OpenscreenStatsClient>();
    session_->SetStatsClient(stats_client_.get());
  }

  // Use of `Unretained` is safe here since we own the update timer.
  bandwidth_update_timer_.Start(
      FROM_HERE, kBandwidthUpdateInterval,
      base::BindRepeating(&OpenscreenSessionHost::UpdateBandwidthEstimate,
                          base::Unretained(this)));
}

OpenscreenSessionHost::~OpenscreenSessionHost() {
  StopSession();

  // If we provided access to our network context proxy, we need to clear it.
  if (set_network_context_proxy_) {
    openscreen_platform::ClearNetworkContextGetter();
  }
}

void OpenscreenSessionHost::AsyncInitialize(
    AsyncInitializedCallback initialized_cb) {
  initialized_cb_ = std::move(initialized_cb);
  if (!gpu_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&OpenscreenSessionHost::OnAsyncInitialized,
                       weak_factory_.GetWeakPtr(), SupportedProfiles{}));
    return;
  }

  gpu_->CreateVideoEncodeAcceleratorProvider(
      vea_provider_.BindNewPipeAndPassReceiver());
  vea_provider_->GetVideoEncodeAcceleratorSupportedProfiles(base::BindOnce(
      &OpenscreenSessionHost::OnAsyncInitialized, weak_factory_.GetWeakPtr()));
}

void OpenscreenSessionHost::OnNegotiated(
    const openscreen::cast::SenderSession* session,
    openscreen::cast::SenderSession::ConfiguredSenders senders,
    Recommendations capture_recommendations) {
  if (state_ == State::kStopped)
    return;

  const media::AudioCodec audio_codec =
      media::cast::ToAudioCodec(senders.audio_config.codec);
  const media::VideoCodec video_codec =
      media::cast::ToVideoCodec(senders.video_config.codec);

  std::optional<FrameSenderConfig> audio_config;
  if (last_offered_audio_config_ && senders.audio_sender) {
    base::UmaHistogramEnumeration("CastStreaming.Sender.Audio.NegotiatedCodec",
                                  audio_codec);
    DCHECK_EQ(last_offered_audio_config_->audio_codec(), audio_codec);
    audio_config = last_offered_audio_config_;
  }

  std::optional<FrameSenderConfig> video_config;
  if (senders.video_sender) {
    base::UmaHistogramEnumeration("CastStreaming.Sender.Video.NegotiatedCodec",
                                  video_codec);

    for (const FrameSenderConfig& config : last_offered_video_configs_) {
      // Since we only offer one configuration per codec, we can determine which
      // config was selected by simply checking its codec.
      if (config.video_codec() == video_codec) {
        video_config = config;
      }
    }
    DCHECK(video_config);

    // Ultimately used by the video encoder that executes on
    // `video_encode_thread_` to determine how many threads should be used to
    // encode video content.
    video_config->video_codec_params.value().number_of_encode_threads =
        NumberOfEncodeThreads();
  }

  // NOTE: the audio and video encode threads are reused across negotiations
  // and should only be instantiated once each.
  const bool initially_starting_session =
      !audio_encode_thread_ && !video_encode_thread_;
  if (initially_starting_session) {
    audio_encode_thread_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
    video_encode_thread_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  }

  cast_environment_ = new media::cast::CastEnvironment(
      base::DefaultTickClock::GetInstance(),
      base::SingleThreadTaskRunner::GetCurrentDefault(), audio_encode_thread_,
      video_encode_thread_);

  if (state_ == State::kRemoting) {
    DCHECK(media_remoter_);
    DCHECK(!audio_config || audio_config->is_remoting());
    DCHECK(!video_config || video_config->is_remoting());

    media_remoter_->StartRpcMessaging(
        cast_environment_, std::move(senders.audio_sender),
        std::move(senders.video_sender), std::move(audio_config),
        std::move(video_config));
    if (session_params_.is_remote_playback) {
      RecordRemotePlaybackSessionLoadTime(remote_playback_start_time_);
      RecordRemotePlaybackSessionStartsBeforeTimeout(true);
      remote_playback_start_timer_.Stop();
    }
    return;
  }

  SetConstraints(capture_recommendations, audio_config, video_config);
  if (senders.audio_sender) {
    auto audio_sender = std::make_unique<media::cast::AudioSender>(
        cast_environment_, *audio_config,
        base::BindOnce(&OpenscreenSessionHost::OnEncoderStatusChange,
                       // Safe because we own `audio_stream`.
                       weak_factory_.GetWeakPtr(), *audio_config),
        std::move(senders.audio_sender));
    audio_stream_ = std::make_unique<AudioRtpStream>(
        std::move(audio_sender), weak_factory_.GetWeakPtr());
    DCHECK(!audio_capturing_callback_);
    // TODO(crbug.com/40103719): Eliminate the thread hops. The audio data is
    // thread-hopped from the audio thread, and later thread-hopped again to
    // the encoding thread.
    audio_capturing_callback_ = std::make_unique<AudioCapturingCallback>(
        base::BindPostTaskToCurrentDefault(base::BindRepeating(
            &AudioRtpStream::InsertAudio, audio_stream_->AsWeakPtr())),
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &OpenscreenSessionHost::ReportAndLogError,
            weak_factory_.GetWeakPtr(), SessionError::AUDIO_CAPTURE_ERROR)),
        observer_);
    audio_input_device_ = new media::AudioInputDevice(
        std::make_unique<CapturedAudioInput>(
            base::BindRepeating(&OpenscreenSessionHost::CreateAudioStream,
                                base::Unretained(this)),
            observer_),
        media::AudioInputDevice::Purpose::kLoopback,
        media::AudioInputDevice::DeadStreamDetection::kEnabled);
    audio_input_device_->Initialize(mirror_settings_.GetAudioCaptureParams(),
                                    audio_capturing_callback_.get());
    audio_input_device_->Start();
  }

  if (senders.video_sender) {
    mojo::PendingRemote<media::mojom::VideoEncoderMetricsProvider>
        metrics_provider_pending_remote;
    resource_provider_->GetVideoEncoderMetricsProvider(
        metrics_provider_pending_remote.InitWithNewPipeAndPassReceiver());
    auto video_sender = std::make_unique<media::cast::VideoSender>(
        cast_environment_, *video_config,
        base::BindRepeating(&OpenscreenSessionHost::OnEncoderStatusChange,
                            weak_factory_.GetWeakPtr(), *video_config),
        base::BindRepeating(
            &OpenscreenSessionHost::CreateVideoEncodeAccelerator,
            weak_factory_.GetWeakPtr()),
        std::move(senders.video_sender),
        base::MakeRefCounted<media::MojoVideoEncoderMetricsProviderFactory>(
            media::mojom::VideoEncoderUseCase::kCastMirroring,
            std::move(metrics_provider_pending_remote))
            ->CreateVideoEncoderMetricsProvider(),
        base::BindRepeating(&OpenscreenSessionHost::SetTargetPlayoutDelay,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&OpenscreenSessionHost::ProcessFeedback,
                            weak_factory_.GetWeakPtr()),
        // This is safe since it is only called synchronously and we own
        // the video sender instance.
        base::BindRepeating(&OpenscreenSessionHost::GetSuggestedVideoBitrate,
                            base::Unretained(this), video_config->min_bitrate,
                            video_config->max_bitrate));
    video_stream_ = std::make_unique<VideoRtpStream>(
        std::move(video_sender), weak_factory_.GetWeakPtr(),
        mirror_settings_.refresh_interval());

    logger_.LogInfo(base::StringPrintf(
        "Created video stream with refresh interval of %d ms",
        static_cast<int>(
            mirror_settings_.refresh_interval().InMilliseconds())));

    if (!video_capture_client_) {
      mojo::PendingRemote<media::mojom::VideoCaptureHost> video_host;
      resource_provider_->GetVideoCaptureHost(
          video_host.InitWithNewPipeAndPassReceiver());
      const media::VideoCaptureParams& capture_params =
          mirror_settings_.GetVideoCaptureParams();
      video_capture_client_ = std::make_unique<VideoCaptureClient>(
          capture_params, std::move(video_host));
      logger_.LogInfo(base::StrCat({"Starting VideoCaptureHost with params ",
                                    ToString(capture_params)}));
      video_capture_client_->Start(
          base::BindRepeating(&VideoRtpStream::InsertVideoFrame,
                              video_stream_->AsWeakPtr()),
          base::BindOnce(&OpenscreenSessionHost::ReportAndLogError,
                         weak_factory_.GetWeakPtr(),
                         SessionError::VIDEO_CAPTURE_ERROR,
                         "VideoCaptureClient reported an error."));
    } else {
      video_capture_client_->Resume(base::BindRepeating(
          &VideoRtpStream::InsertVideoFrame, video_stream_->AsWeakPtr()));
    }
  }

  if (media_remoter_) {
    media_remoter_->OnMirroringResumed(switching_tab_source_);
  }

  switching_tab_source_ = false;

  if (initially_starting_session) {
    if (session_params_.is_remote_playback) {
      // Initialize `media_remoter_` without capabilities for Remote Playback
      // Media Source.
      openscreen::cast::RemotingCapabilities capabilities;
      InitMediaRemoter(capabilities);
      // Hold off video and audio streaming while waiting for the session to
      // switch to Remoting.
      video_capture_client_->Pause();
      audio_input_device_->Stop();
      remote_playback_start_time_ = base::Time::Now();
      remote_playback_start_timer_.Start(
          FROM_HERE, kStartRemotePlaybackTimeOut,
          base::BindOnce(&OpenscreenSessionHost::OnRemotingStartTimeout,
                         weak_factory_.GetWeakPtr()));
    } else {
      // We should only request capabilities once, in order to avoid
      // instantiating the media remoter multiple times.
      session_->RequestCapabilities();
    }
    if (observer_) {
      observer_->DidStart();
    }
  }

  logger_.LogInfo(base::StringPrintf(
      "negotiated a new %s session. audio codec=%s, video codec=%s (%s)",
      (state_ == State::kRemoting ? "remoting" : "mirroring"),
      (audio_config ? media::GetCodecName(audio_config->audio_codec()).c_str()
                    : "none"),
      (video_config ? media::GetCodecName(video_config->video_codec()).c_str()
                    : "none"),
      (video_config
           ? (video_config->use_hardware_encoder ? "hardware" : "software")
           : "n/a")));
}

void OpenscreenSessionHost::OnCapabilitiesDetermined(
    const openscreen::cast::SenderSession* session,
    openscreen::cast::RemotingCapabilities capabilities) {
  DCHECK_EQ(session_.get(), session);

  // This method should only be called once, in order to avoid issues with
  // multiple media remoters getting instantiated and attempting to fulfill the
  // mojom interface. Generally speaking, receivers do not update their remoting
  // capabilities during a single session.
  DCHECK(!media_remoter_);
  if (state_ == State::kStopped)
    return;

  InitMediaRemoter(capabilities);
}

void OpenscreenSessionHost::OnError(
    const openscreen::cast::SenderSession* session,
    const openscreen::Error& error) {
  switch (error.code()) {
    case openscreen::Error::Code::kAnswerTimeout:
      ReportAndLogError(SessionError::ANSWER_TIME_OUT, error.ToString());
      return;

    case openscreen::Error::Code::kInvalidAnswer:
      ReportAndLogError(SessionError::ANSWER_NOT_OK, error.ToString());
      return;

    case openscreen::Error::Code::kNoStreamSelected:
      ReportAndLogError(SessionError::ANSWER_NO_AUDIO_OR_VIDEO,
                        error.ToString());
      return;

    // If remoting is not supported, the session will continue but
    // OnCapabilitiesDetermined() will never be called and the media remoter
    // will not be set up.
    case openscreen::Error::Code::kRemotingNotSupported:
      logger_.LogInfo(base::StrCat(
          {"Remoting is disabled for this session. error=", error.ToString()}));
      return;

    // Default behavior is to report a generic Open Screen session error.
    default:
      ReportAndLogError(SessionError::OPENSCREEN_SESSION_ERROR,
                        error.ToString());
      return;
  }
}

// RtpStreamClient overrides.
void OpenscreenSessionHost::OnError(const std::string& message) {
  ReportAndLogError(SessionError::RTP_STREAM_ERROR, message);
}

void OpenscreenSessionHost::RequestRefreshFrame() {
  if (video_capture_client_)
    video_capture_client_->RequestRefreshFrame();
}

void OpenscreenSessionHost::CreateVideoEncodeAccelerator(
    media::cast::ReceiveVideoEncodeAcceleratorCallback callback) {
  DCHECK_NE(state_, State::kInitializing);
  if (callback.is_null())
    return;

  std::unique_ptr<media::VideoEncodeAccelerator> mojo_vea;
  if (gpu_ && !supported_profiles_.empty()) {
    if (!vea_provider_) {
      gpu_->CreateVideoEncodeAcceleratorProvider(
          vea_provider_.BindNewPipeAndPassReceiver());
    }
    mojo::PendingRemote<media::mojom::VideoEncodeAccelerator> vea;
    vea_provider_->CreateVideoEncodeAccelerator(
        nullptr /* EncodeCommandBufferIdPtr */,
        vea.InitWithNewPipeAndPassReceiver());

    // This is a highly unusual statement due to the fact that
    // `MojoVideoEncodeAccelerator` must be destroyed using `Destroy()` and has
    // a private destructor.
    // TODO(crbug.com/40238884): should be castable to parent type with
    // destructor.
    mojo_vea = base::WrapUnique<media::VideoEncodeAccelerator>(
        new media::MojoVideoEncodeAccelerator(std::move(vea)));
  }
  std::move(callback).Run(base::SingleThreadTaskRunner::GetCurrentDefault(),
                          std::move(mojo_vea));
}

// MediaRemoter::Client overrides.
void OpenscreenSessionHost::ConnectToRemotingSource(
    mojo::PendingRemote<media::mojom::Remoter> remoter,
    mojo::PendingReceiver<media::mojom::RemotingSource> receiver) {
  resource_provider_->ConnectToRemotingSource(std::move(remoter),
                                              std::move(receiver));
}

void OpenscreenSessionHost::RequestRemotingStreaming() {
  DCHECK(media_remoter_);
  DCHECK_EQ(State::kMirroring, state_);
  if (video_capture_client_)
    video_capture_client_->Pause();
  StopStreaming();
  state_ = State::kRemoting;
  Negotiate();
}

void OpenscreenSessionHost::RestartMirroringStreaming() {
  if (state_ != State::kRemoting)
    return;

  // Stop session instead of switching to mirroring when in Remote Playback
  // mode.
  if (session_params_.is_remote_playback) {
    StopSession();
    return;
  }

  StopStreaming();
  state_ = State::kMirroring;
  Negotiate();
}

void OpenscreenSessionHost::SwitchSourceTab() {
  if (observer_)
    observer_->OnSourceChanged();

  if (state_ == State::kRemoting) {
    switching_tab_source_ = true;
    video_capture_client_.reset();
    media_remoter_->Stop(media::mojom::RemotingStopReason::LOCAL_PLAYBACK);
    return;
  }

  DCHECK_EQ(state_, State::kMirroring);

  // Switch video source tab.
  if (video_capture_client_) {
    mojo::PendingRemote<media::mojom::VideoCaptureHost> video_host;
    resource_provider_->GetVideoCaptureHost(
        video_host.InitWithNewPipeAndPassReceiver());
    video_capture_client_->SwitchVideoCaptureHost(std::move(video_host));
  }

  // Switch audio source tab.
  if (audio_input_device_) {
    audio_input_device_->Stop();
    audio_input_device_->Start();
  }

  if (media_remoter_)
    media_remoter_->OnMirroringResumed(true);
}

void OpenscreenSessionHost::OnAsyncInitialized(
    const SupportedProfiles& profiles) {
  if (profiles.empty()) {
    // HW encoding is not supported.
    gpu_.reset();
  } else {
    supported_profiles_ = profiles;
  }

  DCHECK_EQ(state_, State::kInitializing);
  state_ = State::kMirroring;

  Negotiate();
  if (!initialized_cb_.is_null())
    std::move(initialized_cb_).Run();
}

void OpenscreenSessionHost::ReportAndLogError(SessionError error,
                                              std::string_view message) {
  base::UmaHistogramEnumeration("MediaRouter.MirroringService.SessionError",
                                error);
  logger_.LogError(error, message);

  if (state_ == State::kRemoting) {
    // Try to fallback to mirroring.
    media_remoter_->OnRemotingFailed();
    return;
  }

  // Report the error and stop this session.
  if (observer_)
    observer_->OnError(error);

  StopSession();
}

void OpenscreenSessionHost::StopStreaming() {
  logger_.LogInfo(
      base::StrCat({"stopped streaming. state=",
                    base::NumberToString(static_cast<int>(state_))}));
  if (!cast_environment_)
    return;

  if (audio_input_device_) {
    audio_input_device_->Stop();
    audio_input_device_ = nullptr;
  }
  audio_capturing_callback_.reset();
  audio_stream_.reset();
  video_stream_.reset();

  // Since the environment and its properties are ref-counted, this call to
  // release it may not immediately close any of its resources.
  cast_environment_ = nullptr;
}

void OpenscreenSessionHost::StopSession() {
  logger_.LogInfo(
      base::StrCat({"stopped session. state=",
                    base::NumberToString(static_cast<int>(state_))}));
  if (state_ == State::kStopped)
    return;

  state_ = State::kStopped;
  StopStreaming();

  bandwidth_update_timer_.Stop();

  // Notes on order: the media remoter needs to deregister itself from the
  // message dispatcher, which then needs to deregister from the resource
  // provider.
  media_remoter_.reset();
  rpc_dispatcher_.reset();
  audio_encode_thread_.reset();
  video_encode_thread_.reset();
  video_capture_client_.reset();
  resource_provider_.reset();
  gpu_.reset();

  // The session must be reset after all references to it are removed.
  session_.reset();

  weak_factory_.InvalidateWeakPtrs();

  if (observer_) {
    observer_->DidStop();
    observer_.reset();
  }
}

void OpenscreenSessionHost::SetConstraints(
    const Recommendations& recommendations,
    std::optional<FrameSenderConfig>& audio_config,
    std::optional<FrameSenderConfig>& video_config) {
  const auto& audio = recommendations.audio;
  const auto& video = recommendations.video;

  if (video_config) {
    // We use pixels instead of comparing width and height to allow for
    // differences in aspect ratio.
    const int current_pixels =
        mirror_settings_.max_width() * mirror_settings_.max_height();
    const int recommended_pixels = video.maximum.width * video.maximum.height;
    // Prioritize the stricter of the sender's and receiver's constraints.
    if (recommended_pixels < current_pixels) {
      // The resolution constraints here are used to generate the
      // media::VideoCaptureParams below.
      mirror_settings_.SetResolutionConstraints(video.maximum.width,
                                                video.maximum.height);
    }
    video_config->min_bitrate =
        std::max(video_config->min_bitrate, video.bit_rate_limits.minimum);
    video_config->start_bitrate = video_config->min_bitrate;
    video_config->max_bitrate =
        std::min(video_config->max_bitrate, video.bit_rate_limits.maximum);
    video_config->min_playout_delay =
        std::min(video_config->max_playout_delay,
                 base::Milliseconds(video.max_delay.count()));
    video_config->max_frame_rate =
        std::min(video_config->max_frame_rate,
                 static_cast<double>(video.maximum.frame_rate));

    // TODO(crbug.com/1363512): Remove support for sender side letterboxing.
    if (session_params_.force_letterboxing) {
      mirror_settings_.SetSenderSideLetterboxingEnabled(true);
    } else if (base::FeatureList::IsEnabled(
                   features::kCastDisableLetterboxing)) {
      mirror_settings_.SetSenderSideLetterboxingEnabled(false);
    } else {
      // Enable sender-side letterboxing if the receiver specifically does not
      // opt-in to variable aspect ratio video.
      mirror_settings_.SetSenderSideLetterboxingEnabled(
          !video.supports_scaling);
    }
  }

  if (audio_config) {
    audio_config->min_bitrate =
        std::max(audio_config->min_bitrate, audio.bit_rate_limits.minimum);
    audio_config->start_bitrate = audio_config->min_bitrate;
    audio_config->max_bitrate =
        std::min(audio_config->max_bitrate, audio.bit_rate_limits.maximum);
    audio_config->max_playout_delay =
        std::min(audio_config->max_playout_delay,
                 base::Milliseconds(audio.max_delay.count()));
    audio_config->min_playout_delay =
        std::min(audio_config->max_playout_delay,
                 base::Milliseconds(audio.max_delay.count()));
    // Currently, Chrome only supports stereo, so audio.max_channels is ignored.
  }
}

void OpenscreenSessionHost::CreateAudioStream(
    mojo::PendingRemote<mojom::AudioStreamCreatorClient> client,
    const media::AudioParameters& params,
    uint32_t shared_memory_count) {
  resource_provider_->CreateAudioStream(std::move(client), params,
                                        shared_memory_count);
}

void OpenscreenSessionHost::OnEncoderStatusChange(
    const FrameSenderConfig& config,
    OperationalStatus status) {
  const char* error_message = AsErrorMessage(status);
  if (!error_message) {
    return;
  }

  // If we used a hardware video encoder and it failed, denylist it for the rest
  // of the browsing session and try renegotiating.
  if (config.use_hardware_encoder && config.is_video()) {
    CHECK_EQ(state_, State::kMirroring);

    media::cast::encoding_support::DenyListHardwareCodec(config.video_codec());
    StopStreaming();
    Negotiate();

    base::UmaHistogramEnumeration(
        "MediaRouter.MirroringService.DisabledHardwareCodecAndRenegotiated",
        config.video_codec());
    return;
  }

  ReportAndLogError(SessionError::ENCODING_ERROR, error_message);
}

void OpenscreenSessionHost::SetTargetPlayoutDelay(
    base::TimeDelta playout_delay) {
  bool playout_delay_was_updated = false;
  if (audio_stream_ &&
      audio_stream_->GetTargetPlayoutDelay() != playout_delay) {
    audio_stream_->SetTargetPlayoutDelay(playout_delay);
    playout_delay_was_updated = true;
  }

  if (video_stream_ &&
      video_stream_->GetTargetPlayoutDelay() != playout_delay) {
    video_stream_->SetTargetPlayoutDelay(playout_delay);
    playout_delay_was_updated = true;
  }

  if (playout_delay_was_updated) {
    logger_.LogInfo(base::StrCat(
        {"Updated target playout delay to ",
         base::NumberToString(playout_delay.InMilliseconds()), "ms"}));
  }
}

void OpenscreenSessionHost::ProcessFeedback(
    const media::VideoCaptureFeedback& feedback) {
  if (video_capture_client_) {
    video_capture_client_->ProcessFeedback(feedback);
  }
}

int OpenscreenSessionHost::GetSuggestedVideoBitrate(int min_bitrate,
                                                    int max_bitrate) const {
  // First take the suggested bitrate based on the current bandwidth
  // utilization.
  int suggested = usable_bandwidth_;
  if (audio_stream_) {
    suggested -= audio_stream_->GetEncoderBitrate();
  }

  // Then limit it based on the frame sender configuration.
  // TODO(crbug.com/40260069): we should also factor in device
  // capability when determining which bitrate to use.
  return std::clamp(suggested, min_bitrate, max_bitrate);
}

void OpenscreenSessionHost::UpdateBandwidthEstimate() {
  const int bandwidth_estimate = forced_bandwidth_estimate_for_testing_ > 0
                                     ? forced_bandwidth_estimate_for_testing_
                                     : session_->GetEstimatedNetworkBandwidth();

  // Nothing to do yet.
  if (bandwidth_estimate <= 0) {
    return;
  }

  // Don't ever try to use *all* of the network bandwidth! However, don't go
  // below the absolute minimum requirement either.
  constexpr double kGoodNetworkCitizenFactor = 0.8;
  const int usable_bandwidth = std::max<int>(
      kGoodNetworkCitizenFactor * bandwidth_estimate, kMinRequiredBitrate);

  if (usable_bandwidth > usable_bandwidth_) {
    constexpr double kConservativeIncrease = 1.1;
    usable_bandwidth_ = std::min<int>(usable_bandwidth_ * kConservativeIncrease,
                                      usable_bandwidth);
  } else {
    usable_bandwidth_ = usable_bandwidth;
  }

  VLOG(2) << ": updated available bandwidth to " << usable_bandwidth_ << "/"
          << bandwidth_estimate << " ("
          << static_cast<int>(static_cast<float>(usable_bandwidth_) * 100 /
                              bandwidth_estimate)
          << "%).";
}

void OpenscreenSessionHost::Negotiate() {
  switch (state_) {
    case State::kMirroring:
      NegotiateMirroring();
      return;

    case State::kRemoting:
      NegotiateRemoting();
      return;

    case State::kStopped:
    case State::kInitializing:
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void OpenscreenSessionHost::NegotiateMirroring() {
  last_offered_audio_config_ = std::nullopt;
  last_offered_video_configs_.clear();
  std::vector<openscreen::cast::AudioCaptureConfig> audio_configs;
  std::vector<openscreen::cast::VideoCaptureConfig> video_configs;

  if (session_params_.type != SessionType::VIDEO_ONLY) {
    last_offered_audio_config_ = MirrorSettings::GetDefaultAudioConfig(
        RtpPayloadType::AUDIO_OPUS, media::AudioCodec::kOpus);
    UpdateConfigUsingSessionParameters(session_params_,
                                       *last_offered_audio_config_);
    UpdateAudioConfigMaxBitrate(*last_offered_audio_config_);
    audio_configs.push_back(
        ToOpenscreenAudioConfig(*last_offered_audio_config_));
  }

  if (session_params_.type != SessionType::AUDIO_ONLY) {
    // First, check if hardware encoders are available and should be offered.
    const bool should_offer_hardware_vp9 =
        media::cast::encoding_support::IsHardwareEnabled(
            media::VideoCodec::kVP9, supported_profiles_);
    const bool should_offer_hardware_h264 =
        media::cast::encoding_support::IsHardwareEnabled(
            media::VideoCodec::kH264, supported_profiles_);
    const bool should_offer_hardware_vp8 =
        media::cast::encoding_support::IsHardwareEnabled(
            media::VideoCodec::kVP8, supported_profiles_);
    if (should_offer_hardware_vp9) {
      FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
          RtpPayloadType::VIDEO_VP9, media::VideoCodec::kVP9);
      UpdateConfigUsingSessionParameters(session_params_, config);
      config.use_hardware_encoder = true;
      last_offered_video_configs_.push_back(config);
      video_configs.push_back(ToOpenscreenVideoConfig(config));
    }

    if (should_offer_hardware_h264) {
      FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
          RtpPayloadType::VIDEO_H264, media::VideoCodec::kH264);
      UpdateConfigUsingSessionParameters(session_params_, config);
      config.use_hardware_encoder = true;
      last_offered_video_configs_.push_back(config);
      video_configs.push_back(ToOpenscreenVideoConfig(config));
    }

    if (should_offer_hardware_vp8) {
      FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
          RtpPayloadType::VIDEO_VP8, media::VideoCodec::kVP8);
      UpdateConfigUsingSessionParameters(session_params_, config);
      config.use_hardware_encoder = true;
      last_offered_video_configs_.push_back(config);
      video_configs.push_back(ToOpenscreenVideoConfig(config));
    }

    // Then add software AV1 if enabled.
    if (media::cast::encoding_support::IsSoftwareEnabled(
            media::VideoCodec::kAV1)) {
      FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
          RtpPayloadType::VIDEO_AV1, media::VideoCodec::kAV1);
      UpdateConfigUsingSessionParameters(session_params_, config);
      last_offered_video_configs_.push_back(config);
      video_configs.push_back(ToOpenscreenVideoConfig(config));
    }

    // Then add software VP9 if (1) enabled and (2) did not offer hardware
    // encoding.
    if (!should_offer_hardware_vp9 &&
        media::cast::encoding_support::IsSoftwareEnabled(
            media::VideoCodec::kVP9)) {
      FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
          RtpPayloadType::VIDEO_VP9, media::VideoCodec::kVP9);
      UpdateConfigUsingSessionParameters(session_params_, config);
      last_offered_video_configs_.push_back(config);
      video_configs.push_back(ToOpenscreenVideoConfig(config));
    }

    // Finally, offer software VP8 if hardware VP8 was not offered.
    if (!should_offer_hardware_vp8 &&
        media::cast::encoding_support::IsSoftwareEnabled(
            media::VideoCodec::kVP8)) {
      FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
          RtpPayloadType::VIDEO_VP8, media::VideoCodec::kVP8);
      UpdateConfigUsingSessionParameters(session_params_, config);
      last_offered_video_configs_.push_back(config);
      video_configs.push_back(ToOpenscreenVideoConfig(config));
    }
  }

  DCHECK(!audio_configs.empty() || !video_configs.empty());
  session_->Negotiate(audio_configs, video_configs);

  if (observer_) {
    observer_->OnRemotingStateChanged(false);
  }
}

void OpenscreenSessionHost::NegotiateRemoting() {
  FrameSenderConfig audio_config = MirrorSettings::GetDefaultAudioConfig(
      RtpPayloadType::REMOTE_AUDIO, media::AudioCodec::kUnknown);
  UpdateAudioConfigMaxBitrate(audio_config);
  UpdateConfigUsingSessionParameters(session_params_, audio_config);

  FrameSenderConfig video_config = MirrorSettings::GetDefaultVideoConfig(
      RtpPayloadType::REMOTE_VIDEO, media::VideoCodec::kUnknown);
  UpdateConfigUsingSessionParameters(session_params_, video_config);

  last_offered_audio_config_ = audio_config;
  last_offered_video_configs_ = {video_config};

  session_->NegotiateRemoting(ToOpenscreenAudioConfig(audio_config),
                              ToOpenscreenVideoConfig(video_config));

  if (observer_) {
    observer_->OnRemotingStateChanged(true);
  }
}

void OpenscreenSessionHost::InitMediaRemoter(
    const openscreen::cast::RemotingCapabilities& capabilities) {
  rpc_dispatcher_ =
      std::make_unique<RpcDispatcherImpl>(session_->session_messenger());
  media_remoter_ = std::make_unique<MediaRemoter>(
      *this,
      ToRemotingSinkMetadata(capabilities,
                             session_params_.receiver_friendly_name),
      *rpc_dispatcher_);
}

void OpenscreenSessionHost::OnRemotingStartTimeout() {
  if (state_ == State::kRemoting) {
    return;
  }
  StopSession();
  RecordRemotePlaybackSessionStartsBeforeTimeout(false);
}

network::mojom::NetworkContext* OpenscreenSessionHost::GetNetworkContext() {
  return network_context_.get();
}

base::Value::Dict OpenscreenSessionHost::GetMirroringStats() const {
  return stats_client_ ? stats_client_->GetStats() : base::Value::Dict();
}

void OpenscreenSessionHost::SetSenderStatsForTest(
    const openscreen::cast::SenderStats& test_stats) {
  stats_client_->OnStatisticsUpdated(test_stats);
}

}  // namespace mirroring
