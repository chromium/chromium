// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/session.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/cpu.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/mirroring/service/captured_audio_input.h"
#include "components/mirroring/service/mirroring_features.h"
#include "components/mirroring/service/udp_socket_client.h"
#include "components/mirroring/service/video_capture_client.h"
#include "crypto/random.h"
#include "media/audio/audio_input_device.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/bind_to_current_loop.h"
#include "media/cast/encoding/encoding_support.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/sender/audio_sender.h"
#include "media/cast/sender/video_sender.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/mojo/clients/mojo_video_encode_accelerator.h"
#include "media/remoting/device_capability_checker.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/ip_endpoint.h"
#include "services/viz/public/cpp/gpu/gpu.h"
#include "third_party/openscreen/src/cast/streaming/answer_messages.h"
#include "third_party/openscreen/src/cast/streaming/capture_recommendations.h"
#include "third_party/openscreen/src/cast/streaming/offer_messages.h"

using media::cast::CastTransportStatus;
using media::cast::Codec;
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

// The interval for CastTransport to send Frame/PacketEvents to Session for
// logging.
constexpr base::TimeDelta kSendEventsInterval = base::Seconds(1);

// The duration for OFFER/ANSWER exchange. If timeout, notify the client that
// the session failed to start.
constexpr base::TimeDelta kOfferAnswerExchangeTimeout = base::Seconds(15);

// Amount of time to wait before assuming the Cast Receiver does not support
// querying for capabilities via GET_CAPABILITIES.
constexpr base::TimeDelta kGetCapabilitiesTimeout = base::Seconds(30);

// Used for OFFER/ANSWER message exchange. Some receivers will error out on
// payloadType values other than the ones hard-coded here.
constexpr int kAudioPayloadType = 127;
constexpr int kVideoPayloadType = 96;

constexpr int kAudioSsrcMin = 1;
constexpr int kAudioSsrcMax = 5e5;
constexpr int kVideoSsrcMin = 5e5 + 1;
constexpr int kVideoSsrcMax = 10e5;

// The implemented remoting version.
constexpr int kSupportedRemotingVersion = 2;

class TransportClient final : public media::cast::CastTransport::Client {
 public:
  explicit TransportClient(Session* session) : session_(session) {}

  TransportClient(const TransportClient&) = delete;
  TransportClient& operator=(const TransportClient&) = delete;

  ~TransportClient() override {}

  // media::cast::CastTransport::Client implementation.

  void OnStatusChanged(CastTransportStatus status) override {
    session_->OnTransportStatusChanged(status);
  }

  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<FrameEvent>> frame_events,
      std::unique_ptr<std::vector<PacketEvent>> packet_events) override {
    session_->OnLoggingEventsReceived(std::move(frame_events),
                                      std::move(packet_events));
  }

  void ProcessRtpPacket(std::unique_ptr<Packet> packet) override {
    NOTREACHED();
  }

 private:
  const raw_ptr<Session> session_;  // Outlives this class.
};

// Generates a string with cryptographically secure random bytes.
std::string MakeRandomString(size_t length) {
  std::string result(length, ' ');
  crypto::RandBytes(std::data(result), length);
  return result;
}

int NumberOfEncodeThreads() {
  // Do not saturate CPU utilization just for encoding. On a lower-end system
  // with only 1 or 2 cores, use only one thread for encoding. On systems with
  // more cores, allow half of the cores to be used for encoding.
  return std::min(8, (base::SysInfo::NumberOfProcessors() + 1) / 2);
}

// Helper to add |config| to |config_list| with given |aes_key|.
void AddSenderConfig(int32_t sender_ssrc,
                     FrameSenderConfig config,
                     const std::string& aes_key,
                     const std::string& aes_iv,
                     const mojom::SessionParameters& session_params,
                     std::vector<FrameSenderConfig>* config_list) {
  config.aes_key = aes_key;
  config.aes_iv_mask = aes_iv;
  config.sender_ssrc = sender_ssrc;
  if (session_params.target_playout_delay) {
    config.min_playout_delay = *session_params.target_playout_delay;
    config.max_playout_delay = *session_params.target_playout_delay;
  }
  config_list->emplace_back(config);
}

// Generate the stream object from |config| and add it to |stream_list|.
void AddStreamObject(int stream_index,
                     const std::string& codec_name,
                     const FrameSenderConfig& config,
                     const MirrorSettings& mirror_settings,
                     base::Value::List& stream_list) {
  base::Value::Dict stream;
  stream.Set("index", stream_index);
  stream.Set("codecName", base::ToLowerASCII(codec_name));
  stream.Set("rtpProfile", "cast");
  const bool is_audio =
      (config.rtp_payload_type <= media::cast::RtpPayloadType::AUDIO_LAST);
  stream.Set("rtpPayloadType",
             is_audio ? kAudioPayloadType : kVideoPayloadType);
  stream.Set("ssrc", static_cast<int>(config.sender_ssrc));
  stream.Set("targetDelay",
             static_cast<int>(config.max_playout_delay.InMilliseconds()));
  stream.Set("aesKey",
             base::HexEncode(config.aes_key.data(), config.aes_key.size()));
  stream.Set("aesIvMask", base::HexEncode(config.aes_iv_mask.data(),
                                          config.aes_iv_mask.size()));
  stream.Set("timeBase", "1/" + std::to_string(config.rtp_timebase));
  stream.Set("receiverRtcpEventLog", true);
  stream.Set("rtpExtensions", "adaptive_playout_delay");
  if (is_audio) {
    // Note on "AUTO" bitrate calculation: This is based on libopus source
    // at the time of this writing. Internally, it uses the following math:
    //
    //   packet_overhead_bps = 60 bits * num_packets_in_one_second
    //   approx_encoded_signal_bps = frequency * channels
    //   estimated_bps = packet_overhead_bps + approx_encoded_signal_bps
    //
    // For 100 packets/sec at 48 kHz and 2 channels, this is 102kbps.
    const int bitrate = config.max_bitrate > 0
                            ? config.max_bitrate
                            : (60 * config.max_frame_rate +
                               config.rtp_timebase * config.channels);
    stream.Set("type", "audio_source");
    stream.Set("bitRate", bitrate);
    stream.Set("sampleRate", config.rtp_timebase);
    stream.Set("channels", config.channels);
  } else /* is video */ {
    stream.Set("type", "video_source");
    stream.Set("renderMode", "video");
    stream.Set("maxFrameRate",
               std::to_string(static_cast<int>(config.max_frame_rate * 1000)) +
                   "/1000");
    stream.Set("maxBitRate", config.max_bitrate);
    base::Value::List resolutions;
    base::Value::Dict resolution;
    resolution.Set("width", mirror_settings.max_width());
    resolution.Set("height", mirror_settings.max_height());
    resolutions.Append(std::move(resolution));
    stream.Set("resolutions", std::move(resolutions));
  }
  stream_list.Append(std::move(stream));
}

// Convert the sink capabilities to media::mojom::RemotingSinkMetadata.
media::mojom::RemotingSinkMetadata ToRemotingSinkMetadata(
    const std::vector<std::string>& capabilities,
    const mojom::SessionParameters& params) {
  media::mojom::RemotingSinkMetadata sink_metadata;
  sink_metadata.friendly_name = params.receiver_friendly_name;

  for (const auto& capability : capabilities) {
    if (capability == "audio") {
      sink_metadata.audio_capabilities.push_back(
          RemotingSinkAudioCapability::CODEC_BASELINE_SET);
    } else if (capability == "aac") {
      sink_metadata.audio_capabilities.push_back(
          RemotingSinkAudioCapability::CODEC_AAC);
    } else if (capability == "opus") {
      sink_metadata.audio_capabilities.push_back(
          RemotingSinkAudioCapability::CODEC_OPUS);
    } else if (capability == "video") {
      sink_metadata.video_capabilities.push_back(
          RemotingSinkVideoCapability::CODEC_BASELINE_SET);
    } else if (capability == "4k") {
      sink_metadata.video_capabilities.push_back(
          RemotingSinkVideoCapability::SUPPORT_4K);
    } else if (capability == "h264") {
      sink_metadata.video_capabilities.push_back(
          RemotingSinkVideoCapability::CODEC_H264);
    } else if (capability == "vp8") {
      sink_metadata.video_capabilities.push_back(
          RemotingSinkVideoCapability::CODEC_VP8);
    } else if (capability == "vp9") {
      // TODO(crbug.com/1198616): receiver_model_name hacks should be removed.
      if (base::StartsWith(params.receiver_model_name, "Chromecast Ultra",
                           base::CompareCase::SENSITIVE)) {
        sink_metadata.video_capabilities.push_back(
            RemotingSinkVideoCapability::CODEC_VP9);
      }
    } else if (capability == "hevc") {
      // TODO(crbug.com/1198616): receiver_model_name hacks should be removed.
      if (base::StartsWith(params.receiver_model_name, "Chromecast Ultra",
                           base::CompareCase::SENSITIVE)) {
        sink_metadata.video_capabilities.push_back(
            RemotingSinkVideoCapability::CODEC_HEVC);
      }
    } else {
      DVLOG(1) << "Unknown mediaCap name: " << capability;
    }
  }

  // Enable remoting 1080p 30fps or higher resolution/fps content for Chromecast
  // Ultra receivers only.
  // TODO(crbug.com/1198616): receiver_model_name hacks should be removed.
  if (params.receiver_model_name == "Chromecast Ultra") {
    sink_metadata.video_capabilities.push_back(
        RemotingSinkVideoCapability::SUPPORT_4K);
  }

  return sink_metadata;
}

// TODO(crbug.com/1198616): Remove this.
bool ShouldQueryForRemotingCapabilities(
    const std::string& receiver_model_name) {
  if (base::FeatureList::IsEnabled(features::kCastDisableModelNameCheck))
    return true;

  return media::remoting::IsKnownToSupportRemoting(receiver_model_name);
}

const std::string ToString(const media::VideoCaptureParams& params) {
  return base::StringPrintf(
      "requested_format = %s, buffer_type = %d, resolution_policy = %d",
      media::VideoCaptureFormat::ToString(params.requested_format).c_str(),
      static_cast<int>(params.buffer_type),
      static_cast<int>(params.resolution_change_policy));
}

}  // namespace

class Session::AudioCapturingCallback final
    : public media::AudioCapturerSource::CaptureCallback {
 public:
  using AudioDataCallback =
      base::RepeatingCallback<void(std::unique_ptr<media::AudioBus> audio_bus,
                                   base::TimeTicks recorded_time)>;
  AudioCapturingCallback(AudioDataCallback audio_data_callback,
                         base::OnceClosure error_callback)
      : audio_data_callback_(std::move(audio_data_callback)),
        error_callback_(std::move(error_callback)) {
    DCHECK(!audio_data_callback_.is_null());
  }

  AudioCapturingCallback(const AudioCapturingCallback&) = delete;
  AudioCapturingCallback& operator=(const AudioCapturingCallback&) = delete;

  ~AudioCapturingCallback() override {}

 private:
  // media::AudioCapturerSource::CaptureCallback implementation.
  void OnCaptureStarted() override {}

  // Called on audio thread.
  void Capture(const media::AudioBus* audio_bus,
               base::TimeTicks audio_capture_time,
               double volume,
               bool key_pressed) override {
    // TODO(crbug.com/1015467): Don't copy the audio data. Instead, send
    // |audio_bus| directly to the encoder.
    std::unique_ptr<media::AudioBus> captured_audio =
        media::AudioBus::Create(audio_bus->channels(), audio_bus->frames());
    audio_bus->CopyTo(captured_audio.get());
    audio_data_callback_.Run(std::move(captured_audio), audio_capture_time);
  }

  void OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                      const std::string& message) override {
    if (!error_callback_.is_null())
      std::move(error_callback_).Run();
  }

  void OnCaptureMuted(bool is_muted) override {}

  const AudioDataCallback audio_data_callback_;
  base::OnceClosure error_callback_;
};

Session::Session(
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
      message_dispatcher_(std::make_unique<MessageDispatcher>(
          std::move(outbound_channel),
          std::move(inbound_channel),
          base::BindRepeating(&Session::OnResponseParsingError,
                              base::Unretained(this)))) {
  DCHECK(resource_provider_);
  mirror_settings_.SetResolutionConstraints(max_resolution.width(),
                                            max_resolution.height());

  if (session_params_.refresh_interval) {
    mirror_settings_.set_refresh_interval(*(session_params_.refresh_interval));
  }

  resource_provider_->GetNetworkContext(
      network_context_.BindNewPipeAndPassReceiver());

  if (session_params->type != mojom::SessionType::AUDIO_ONLY &&
      io_task_runner) {
    mojo::PendingRemote<viz::mojom::Gpu> remote_gpu;
    resource_provider_->BindGpu(remote_gpu.InitWithNewPipeAndPassReceiver());
    gpu_ = viz::Gpu::Create(std::move(remote_gpu), io_task_runner);
  }
}

void Session::AsyncInitialize(AsyncInitializeDoneCB done_cb) {
  init_done_cb_ = std::move(done_cb);
  if (!gpu_) {
    // Post OnAsyncInitializeDone() instead of calling it directly to make sure
    // that CreateAndSendOffer() is always called asynchronously. This kind of
    // consistency is good for testing and reliability.
    auto runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    SupportedProfiles empty_profiles;
    runner->PostTask(FROM_HERE, base::BindOnce(&Session::OnAsyncInitializeDone,
                                               weak_factory_.GetWeakPtr(),
                                               std::move(empty_profiles)));
    return;
  }

  gpu_->CreateVideoEncodeAcceleratorProvider(
      vea_provider_.BindNewPipeAndPassReceiver());
  vea_provider_->GetVideoEncodeAcceleratorSupportedProfiles(base::BindOnce(
      &Session::OnAsyncInitializeDone, weak_factory_.GetWeakPtr()));
}

void Session::OnAsyncInitializeDone(const SupportedProfiles& profiles) {
  if (profiles.empty()) {
    // HW encoding is not supported.
    gpu_.reset();
  } else {
    supported_profiles_ = profiles;
  }
  DCHECK_EQ(state_, INITIALIZING);
  state_ = MIRRORING;

  CreateAndSendOffer();
  if (!init_done_cb_.is_null())
    std::move(init_done_cb_).Run();
}

Session::~Session() {
  StopSession();
}

void Session::ReportError(SessionError error) {
  UMA_HISTOGRAM_ENUMERATION("MediaRouter.MirroringService.SessionError", error);
  if (state_ == REMOTING) {
    media_remoter_->OnRemotingFailed();  // Try to fallback on mirroring.
    return;
  }

  // Report the error and stop this session.
  if (observer_)
    observer_->OnError(error);
  StopSession();
}

void Session::LogInfoMessage(const std::string& message) {
  if (observer_) {
    observer_->LogInfoMessage(message);
  }
}

void Session::LogErrorMessage(const std::string& message) {
  if (observer_) {
    observer_->LogErrorMessage(message);
  }
}
void Session::StopStreaming() {
  DVLOG(2) << __func__ << " state=" << state_;
  if (!cast_environment_)
    return;

  if (audio_input_device_) {
    audio_input_device_->Stop();
    audio_input_device_ = nullptr;
  }
  audio_capturing_callback_.reset();
  audio_stream_.reset();
  video_stream_.reset();
  cast_transport_.reset();
  cast_environment_ = nullptr;
}

void Session::StopSession() {
  DVLOG(1) << __func__;
  if (state_ == STOPPED)
    return;

  state_ = STOPPED;
  StopStreaming();

  // Notes on order: the media remoter needs to deregister itself from the
  // message dispatcher, which then needs to deregister from the resource
  // provider.
  media_remoter_.reset();
  message_dispatcher_.reset();
  rpc_dispatcher_.reset();
  weak_factory_.InvalidateWeakPtrs();
  audio_encode_thread_ = nullptr;
  video_encode_thread_ = nullptr;
  video_capture_client_.reset();
  resource_provider_.reset();
  gpu_.reset();
  if (observer_) {
    observer_->DidStop();
    observer_.reset();
  }
}

void Session::OnError(const std::string& message) {
  ReportError(SessionError::RTP_STREAM_ERROR);
}

void Session::RequestRefreshFrame() {
  DVLOG(3) << __func__;
  if (video_capture_client_)
    video_capture_client_->RequestRefreshFrame();
}

void Session::OnEncoderStatusChange(OperationalStatus status) {
  switch (status) {
    case OperationalStatus::STATUS_UNINITIALIZED:
    case OperationalStatus::STATUS_CODEC_REINIT_PENDING:
    // Not an error.
    // TODO(crbug.com/1015467): As an optimization, signal the client to pause
    // sending more frames until the state becomes STATUS_INITIALIZED again.
    case OperationalStatus::STATUS_INITIALIZED:
      break;
    case OperationalStatus::STATUS_INVALID_CONFIGURATION:
    case OperationalStatus::STATUS_UNSUPPORTED_CODEC:
    case OperationalStatus::STATUS_CODEC_INIT_FAILED:
    case OperationalStatus::STATUS_CODEC_RUNTIME_ERROR:
      ReportError(SessionError::ENCODING_ERROR);
      break;
  }
}

void Session::CreateVideoEncodeAccelerator(
    media::cast::ReceiveVideoEncodeAcceleratorCallback callback) {
  DCHECK_NE(state_, INITIALIZING);
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
        vea.InitWithNewPipeAndPassReceiver());
    // std::make_unique doesn't work to create a unique pointer of the subclass.
    mojo_vea = base::WrapUnique<media::VideoEncodeAccelerator>(
        new media::MojoVideoEncodeAccelerator(std::move(vea)));
  }
  std::move(callback).Run(base::SingleThreadTaskRunner::GetCurrentDefault(),
                          std::move(mojo_vea));
}

void Session::OnTransportStatusChanged(CastTransportStatus status) {
  DVLOG(1) << __func__ << ": status=" << status;
  switch (status) {
    case CastTransportStatus::TRANSPORT_STREAM_UNINITIALIZED:
    case CastTransportStatus::TRANSPORT_STREAM_INITIALIZED:
      return;  // Not errors, do nothing.
    case CastTransportStatus::TRANSPORT_INVALID_CRYPTO_CONFIG:
    case CastTransportStatus::TRANSPORT_SOCKET_ERROR:
      ReportError(SessionError::CAST_TRANSPORT_ERROR);
      break;
  }
}

void Session::OnLoggingEventsReceived(
    std::unique_ptr<std::vector<FrameEvent>> frame_events,
    std::unique_ptr<std::vector<PacketEvent>> packet_events) {
  DCHECK(cast_environment_);
  cast_environment_->logger()->DispatchBatchOfEvents(std::move(frame_events),
                                                     std::move(packet_events));
}

void Session::ApplyConstraintsToConfigs(
    const openscreen::cast::Answer& answer,
    absl::optional<FrameSenderConfig>& audio_config,
    absl::optional<FrameSenderConfig>& video_config) {
  const auto recommendations =
      openscreen::cast::capture_recommendations::GetRecommendations(answer);
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
    video_config->max_playout_delay =
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
    // Currently, Chrome only supports stereo, so audio.max_channels is ignored.
  }
}

void Session::OnAnswer(const std::vector<FrameSenderConfig>& audio_configs,
                       const std::vector<FrameSenderConfig>& video_configs,
                       const ReceiverResponse& response) {
  if (state_ == STOPPED)
    return;

  if (response.type() == ResponseType::UNKNOWN) {
    ReportError(SessionError::ANSWER_TIME_OUT);
    return;
  }

  DCHECK_EQ(ResponseType::ANSWER, response.type());
  if (!response.valid()) {
    ReportError(SessionError::ANSWER_NOT_OK);
    return;
  }

  const openscreen::cast::Answer& answer = response.answer();
  if (answer.send_indexes.size() != answer.ssrcs.size()) {
    ReportError(SessionError::ANSWER_MISMATCHED_SSRC_LENGTH);
    return;
  }

  // Select Audio/Video config from ANSWER.
  bool has_audio = false;
  bool has_video = false;
  absl::optional<FrameSenderConfig> audio_config;
  absl::optional<FrameSenderConfig> video_config;
  const int video_start_idx = audio_configs.size();
  const int video_idx_bound = video_configs.size() + video_start_idx;
  for (size_t i = 0; i < answer.send_indexes.size(); ++i) {
    if (answer.send_indexes[i] < 0 ||
        answer.send_indexes[i] >= video_idx_bound) {
      ReportError(SessionError::ANSWER_SELECT_INVALID_INDEX);
      return;
    }
    if (answer.send_indexes[i] < video_start_idx) {
      // Audio
      if (has_audio) {
        ReportError(SessionError::ANSWER_SELECT_MULTIPLE_AUDIO);
        return;
      }
      audio_config = audio_configs[answer.send_indexes[i]];
      audio_config->receiver_ssrc = answer.ssrcs[i];
      has_audio = true;
    } else {
      // Video
      if (has_video) {
        ReportError(SessionError::ANSWER_SELECT_MULTIPLE_VIDEO);
        return;
      }
      video_config = video_configs[answer.send_indexes[i] - video_start_idx];
      video_config->receiver_ssrc = answer.ssrcs[i];
      video_config->video_codec_params.number_of_encode_threads =
          NumberOfEncodeThreads();
      has_video = true;
    }
  }
  if (!has_audio && !has_video) {
    ReportError(SessionError::ANSWER_NO_AUDIO_OR_VIDEO);
    return;
  }

  // Set constraints from ANSWER message.
  ApplyConstraintsToConfigs(answer, audio_config, video_config);

  // Start streaming.
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
  auto udp_client = std::make_unique<UdpSocketClient>(
      net::IPEndPoint(session_params_.receiver_address, answer.udp_port),
      network_context_.get(),
      base::BindOnce(&Session::ReportError, weak_factory_.GetWeakPtr(),
                     SessionError::CAST_TRANSPORT_ERROR));
  cast_transport_ = media::cast::CastTransport::Create(
      cast_environment_->Clock(), kSendEventsInterval,
      std::make_unique<TransportClient>(this), std::move(udp_client),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  if (state_ == REMOTING) {
    DCHECK(media_remoter_);
    DCHECK(!audio_config ||
           audio_config->rtp_payload_type == RtpPayloadType::REMOTE_AUDIO);
    DCHECK(!video_config ||
           video_config->rtp_payload_type == RtpPayloadType::REMOTE_VIDEO);
    media_remoter_->StartRpcMessaging(cast_environment_, cast_transport_.get(),
                                      audio_config, video_config);
  } else /* MIRRORING */ {
    if (has_audio) {
      auto audio_sender = std::make_unique<media::cast::AudioSender>(
          cast_environment_, *audio_config,
          base::BindOnce(&Session::OnEncoderStatusChange,
                         weak_factory_.GetWeakPtr()),
          cast_transport_.get());
      audio_stream_ = std::make_unique<AudioRtpStream>(
          std::move(audio_sender), weak_factory_.GetWeakPtr());
      DCHECK(!audio_capturing_callback_);
      // TODO(crbug.com/1015467): Eliminate the thread hops. The audio data is
      // thread-hopped from the audio thread, and later thread-hopped again to
      // the encoding thread.
      audio_capturing_callback_ = std::make_unique<AudioCapturingCallback>(
          media::BindToCurrentLoop(base::BindRepeating(
              &AudioRtpStream::InsertAudio, audio_stream_->AsWeakPtr())),
          base::BindOnce(&Session::ReportError, weak_factory_.GetWeakPtr(),
                         SessionError::AUDIO_CAPTURE_ERROR));
      audio_input_device_ = new media::AudioInputDevice(
          std::make_unique<CapturedAudioInput>(base::BindRepeating(
              &Session::CreateAudioStream, base::Unretained(this))),
          media::AudioInputDevice::Purpose::kLoopback,
          media::AudioInputDevice::DeadStreamDetection::kEnabled);
      const media::AudioParameters& capture_params =
          mirror_settings_.GetAudioCaptureParams();
      LogInfoMessage(base::StrCat({"Creating AudioInputDevice with params ",
                                   capture_params.AsHumanReadableString()}));
      audio_input_device_->Initialize(capture_params,
                                      audio_capturing_callback_.get());
      audio_input_device_->Start();
    }

    if (has_video) {
      auto video_sender = std::make_unique<media::cast::VideoSender>(
          cast_environment_, *video_config,
          base::BindRepeating(&Session::OnEncoderStatusChange,
                              weak_factory_.GetWeakPtr()),
          base::BindRepeating(&Session::CreateVideoEncodeAccelerator,
                              weak_factory_.GetWeakPtr()),
          cast_transport_.get(),
          base::BindRepeating(&Session::SetTargetPlayoutDelay,
                              weak_factory_.GetWeakPtr()),
          base::BindRepeating(&Session::ProcessFeedback,
                              weak_factory_.GetWeakPtr()));
      video_stream_ = std::make_unique<VideoRtpStream>(
          std::move(video_sender), weak_factory_.GetWeakPtr(),
          mirror_settings_.refresh_interval());
      if (!video_capture_client_) {
        mojo::PendingRemote<media::mojom::VideoCaptureHost> video_host;
        resource_provider_->GetVideoCaptureHost(
            video_host.InitWithNewPipeAndPassReceiver());
        const media::VideoCaptureParams& capture_params =
            mirror_settings_.GetVideoCaptureParams();
        LogInfoMessage(base::StrCat({"Starting VideoCaptureHost with params ",
                                     ToString(capture_params)}));
        video_capture_client_ = std::make_unique<VideoCaptureClient>(
            mirror_settings_.GetVideoCaptureParams(), std::move(video_host));
        video_capture_client_->Start(
            base::BindRepeating(&VideoRtpStream::InsertVideoFrame,
                                video_stream_->AsWeakPtr()),
            base::BindOnce(&Session::ReportError, weak_factory_.GetWeakPtr(),
                           SessionError::VIDEO_CAPTURE_ERROR));
      } else {
        video_capture_client_->Resume(base::BindRepeating(
            &VideoRtpStream::InsertVideoFrame, video_stream_->AsWeakPtr()));
      }
    }
    if (media_remoter_)
      media_remoter_->OnMirroringResumed(switching_tab_source_);

    switching_tab_source_ = false;
  }

  if (initially_starting_session) {
    if (session_params_.is_remote_playback) {
      InitMediaRemoter({});
    } else if (ShouldQueryForRemotingCapabilities(
                   session_params_.receiver_model_name)) {
      QueryCapabilitiesForRemoting();
    }
  }

  if (initially_starting_session && observer_)
    observer_->DidStart();
}

void Session::OnResponseParsingError(const std::string& error_message) {
  LogErrorMessage(base::StrCat({"MessageDispatcher error: ", error_message}));
}

void Session::CreateAudioStream(
    mojo::PendingRemote<mojom::AudioStreamCreatorClient> client,
    const media::AudioParameters& params,
    uint32_t shared_memory_count) {
  resource_provider_->CreateAudioStream(std::move(client), params,
                                        shared_memory_count);
}

void Session::SetTargetPlayoutDelay(base::TimeDelta playout_delay) {
  if (audio_stream_)
    audio_stream_->SetTargetPlayoutDelay(playout_delay);
  if (video_stream_)
    video_stream_->SetTargetPlayoutDelay(playout_delay);
}

void Session::ProcessFeedback(const media::VideoCaptureFeedback& feedback) {
  if (video_capture_client_) {
    video_capture_client_->ProcessFeedback(feedback);
  }
}

// TODO(issuetracker.google.com/159352836): Refactor to use libcast's
// OFFER message format.
void Session::CreateAndSendOffer() {
  DCHECK_NE(state_, STOPPED);
  DCHECK_NE(state_, INITIALIZING);

  // The random AES key and initialization vector pair used by all streams in
  // this session.
  const std::string aes_key = MakeRandomString(16);  // AES-128.
  const std::string aes_iv = MakeRandomString(16);   // AES has 128-bit blocks.
  std::vector<FrameSenderConfig> audio_configs;
  std::vector<FrameSenderConfig> video_configs;

  // Generate stream list with supported audio / video configs.
  base::Value::List stream_list;
  int stream_index = 0;
  if (session_params_.type != SessionType::VIDEO_ONLY) {
    const int32_t audio_ssrc = base::RandInt(kAudioSsrcMin, kAudioSsrcMax);
    if (state_ == MIRRORING) {
      FrameSenderConfig config = MirrorSettings::GetDefaultAudioConfig(
          RtpPayloadType::AUDIO_OPUS, Codec::CODEC_AUDIO_OPUS);
      AddSenderConfig(audio_ssrc, config, aes_key, aes_iv, session_params_,
                      &audio_configs);
      AddStreamObject(stream_index++, "OPUS", audio_configs.back(),
                      mirror_settings_, stream_list);
    } else /* REMOTING */ {
      FrameSenderConfig config = MirrorSettings::GetDefaultAudioConfig(
          RtpPayloadType::REMOTE_AUDIO, Codec::CODEC_AUDIO_REMOTE);
      AddSenderConfig(audio_ssrc, config, aes_key, aes_iv, session_params_,
                      &audio_configs);
      AddStreamObject(stream_index++, "REMOTE_AUDIO", audio_configs.back(),
                      mirror_settings_, stream_list);
    }
  }
  if (session_params_.type != SessionType::AUDIO_ONLY) {
    const int32_t video_ssrc = base::RandInt(kVideoSsrcMin, kVideoSsrcMax);
    if (state_ == MIRRORING) {
      // First, check if hardware VP8 and H264 are available.
      const bool should_offer_hardware_vp8 =
          media::cast::encoding_support::IsHardwareEnabled(
              Codec::CODEC_VIDEO_VP8, supported_profiles_);

      if (should_offer_hardware_vp8) {
        FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
            RtpPayloadType::VIDEO_VP8, Codec::CODEC_VIDEO_VP8);
        config.use_hardware_encoder = true;
        AddSenderConfig(video_ssrc, config, aes_key, aes_iv, session_params_,
                        &video_configs);
        AddStreamObject(stream_index++, "VP8", video_configs.back(),
                        mirror_settings_, stream_list);
      }

      if (media::cast::encoding_support::IsHardwareEnabled(
              Codec::CODEC_VIDEO_H264, supported_profiles_)) {
        FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
            RtpPayloadType::VIDEO_H264, Codec::CODEC_VIDEO_H264);
        config.use_hardware_encoder = true;
        AddSenderConfig(video_ssrc, config, aes_key, aes_iv, session_params_,
                        &video_configs);
        AddStreamObject(stream_index++, "H264", video_configs.back(),
                        mirror_settings_, stream_list);
      }

      // Then add software AV1 and VP9 if enabled.
      if (media::cast::encoding_support::IsSoftwareEnabled(
              Codec::CODEC_VIDEO_AV1)) {
        FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
            RtpPayloadType::VIDEO_AV1, Codec::CODEC_VIDEO_AV1);
        AddSenderConfig(video_ssrc, config, aes_key, aes_iv, session_params_,
                        &video_configs);
        AddStreamObject(stream_index++, "AV1", video_configs.back(),
                        mirror_settings_, stream_list);
      }

      if (media::cast::encoding_support::IsSoftwareEnabled(
              Codec::CODEC_VIDEO_VP9)) {
        FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
            RtpPayloadType::VIDEO_VP9, Codec::CODEC_VIDEO_VP9);
        AddSenderConfig(video_ssrc, config, aes_key, aes_iv, session_params_,
                        &video_configs);
        AddStreamObject(stream_index++, "VP9", video_configs.back(),
                        mirror_settings_, stream_list);
      }

      // Finally, offer software VP8 if hardware VP8 was not offered.
      if (!should_offer_hardware_vp8 &&
          media::cast::encoding_support::IsSoftwareEnabled(
              Codec::CODEC_VIDEO_VP8)) {
        FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
            RtpPayloadType::VIDEO_VP8, Codec::CODEC_VIDEO_VP8);
        AddSenderConfig(video_ssrc, config, aes_key, aes_iv, session_params_,
                        &video_configs);
        AddStreamObject(stream_index++, "VP8", video_configs.back(),
                        mirror_settings_, stream_list);
      }

    } else /* REMOTING */ {
      FrameSenderConfig config = MirrorSettings::GetDefaultVideoConfig(
          RtpPayloadType::REMOTE_VIDEO, Codec::CODEC_VIDEO_REMOTE);
      AddSenderConfig(video_ssrc, config, aes_key, aes_iv, session_params_,
                      &video_configs);
      AddStreamObject(stream_index++, "REMOTE_VIDEO", video_configs.back(),
                      mirror_settings_, stream_list);
    }
  }
  DCHECK(!audio_configs.empty() || !video_configs.empty());

  // Assemble the OFFER message.
  base::Value::Dict offer;
  offer.Set("castMode", state_ == MIRRORING ? "mirroring" : "remoting");
  offer.Set("receiverGetStatus", true);
  offer.Set("supportedStreams", std::move(stream_list));

  const int32_t sequence_number = message_dispatcher_->GetNextSeqNumber();
  base::Value::Dict offer_message;
  offer_message.Set("type", "OFFER");
  offer_message.Set("seqNum", sequence_number);
  offer_message.Set("offer", std::move(offer));

  mojom::CastMessagePtr message_to_receiver = mojom::CastMessage::New();
  message_to_receiver->message_namespace = mojom::kWebRtcNamespace;
  const bool did_serialize_offer = base::JSONWriter::Write(
      offer_message, &message_to_receiver->json_format_data);
  DCHECK(did_serialize_offer);

  message_dispatcher_->RequestReply(
      std::move(message_to_receiver), ResponseType::ANSWER, sequence_number,
      kOfferAnswerExchangeTimeout,
      base::BindOnce(&Session::OnAnswer, base::Unretained(this), audio_configs,
                     video_configs));
}

void Session::ConnectToRemotingSource(
    mojo::PendingRemote<media::mojom::Remoter> remoter,
    mojo::PendingReceiver<media::mojom::RemotingSource> receiver) {
  resource_provider_->ConnectToRemotingSource(std::move(remoter),
                                              std::move(receiver));
}

void Session::RequestRemotingStreaming() {
  DCHECK(media_remoter_);
  DCHECK_EQ(MIRRORING, state_);
  if (video_capture_client_)
    video_capture_client_->Pause();
  StopStreaming();
  state_ = REMOTING;
  CreateAndSendOffer();
}

void Session::RestartMirroringStreaming() {
  if (state_ != REMOTING)
    return;

  // Stop session instead of switching to mirroring when in Remote Playback
  // mode.
  if (session_params_.is_remote_playback) {
    StopSession();
    return;
  }

  StopStreaming();
  state_ = MIRRORING;
  CreateAndSendOffer();
}

void Session::SwitchSourceTab() {
  if (observer_)
    observer_->OnSourceChanged();

  if (state_ == REMOTING) {
    switching_tab_source_ = true;
    video_capture_client_.reset();
    media_remoter_->Stop(media::mojom::RemotingStopReason::LOCAL_PLAYBACK);
    return;
  }

  DCHECK_EQ(state_, MIRRORING);

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

void Session::QueryCapabilitiesForRemoting() {
  DCHECK(!media_remoter_);
  const int32_t sequence_number = message_dispatcher_->GetNextSeqNumber();
  base::Value::Dict query;
  query.Set("type", "GET_CAPABILITIES");
  query.Set("seqNum", sequence_number);

  mojom::CastMessagePtr query_message = mojom::CastMessage::New();
  query_message->message_namespace = mojom::kWebRtcNamespace;
  const bool did_serialize_query =
      base::JSONWriter::Write(query, &query_message->json_format_data);
  DCHECK(did_serialize_query);
  message_dispatcher_->RequestReply(
      std::move(query_message), ResponseType::CAPABILITIES_RESPONSE,
      sequence_number, kGetCapabilitiesTimeout,
      base::BindOnce(&Session::OnCapabilitiesResponse, base::Unretained(this)));
}

void Session::InitMediaRemoter(const std::vector<std::string>& caps) {
  DCHECK(!media_remoter_);
  rpc_dispatcher_ = std::make_unique<RpcDispatcherImpl>(*message_dispatcher_);
  media_remoter_ = std::make_unique<MediaRemoter>(
      *this, ToRemotingSinkMetadata(caps, session_params_), *rpc_dispatcher_);
}

void Session::OnCapabilitiesResponse(const ReceiverResponse& response) {
  if (state_ == STOPPED)
    return;

  if (!response.valid()) {
    if (response.error()) {
      LogErrorMessage(base::StringPrintf(
          "Remoting is not supported. Error code: %d, description: %s, "
          "details: %s",
          response.error()->code, response.error()->description.c_str(),
          response.error()->details.c_str()));
    } else {
      LogErrorMessage("Remoting is not supported. Bad CAPABILITIES_RESPONSE.");
    }
    return;
  }

  // Check if the remoting version used in the receiver is supported or not.
  int remoting_version = response.capabilities().remoting;

  // For backwards-compatibility, if the remoting version field was not set,
  // assume it is 1.
  if (remoting_version == ReceiverCapability::kRemotingVersionUnknown) {
    remoting_version = 1;
  }

  if (remoting_version > kSupportedRemotingVersion) {
    LogErrorMessage(
        base::StringPrintf("Remoting is not supported. The receiver's remoting "
                           "version (%d) is not supported by the sender (%d).",
                           remoting_version, kSupportedRemotingVersion));
    return;
  }

  InitMediaRemoter(response.capabilities().media_caps);
}

}  // namespace mirroring
