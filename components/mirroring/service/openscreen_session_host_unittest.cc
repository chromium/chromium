// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/openscreen_session_host.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/mirroring/service/fake_network_service.h"
#include "components/mirroring/service/fake_video_capture_host.h"
#include "components/mirroring/service/mirror_settings.h"
#include "components/mirroring/service/mirroring_features.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/cast/cast_config.h"
#include "media/cast/encoding/encoding_support.h"
#include "media/cast/test/utility/default_config.h"
#include "media/video/video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/viz/public/cpp/gpu/gpu.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/writer.h"
#include "third_party/openscreen/src/cast/streaming/message_fields.h"
#include "third_party/openscreen/src/cast/streaming/public/offer_messages.h"
#include "third_party/openscreen/src/cast/streaming/remoting_capabilities.h"
#include "third_party/openscreen/src/cast/streaming/sender_message.h"
#include "third_party/openscreen/src/cast/streaming/ssrc.h"

using media::cast::FrameSenderConfig;
using media::cast::Packet;
using media::mojom::RemotingSinkMetadata;
using media::mojom::RemotingSinkMetadataPtr;
using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingStopReason;
using mirroring::mojom::SessionError;
using mirroring::mojom::SessionType;
using openscreen::ErrorOr;
using openscreen::cast::SenderMessage;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::NiceMock;

namespace mirroring {

namespace {


const openscreen::cast::Answer kAnswerWithConstraints{
    1234,
    // Send indexes and SSRCs are set later.
    {},
    {},
    openscreen::cast::Constraints{
        openscreen::cast::AudioConstraints{44100, 2, 32000, 960000,
                                           std::chrono::milliseconds(4000)},
        openscreen::cast::VideoConstraints{
            40000.0, openscreen::cast::Dimensions{320, 480, {30, 1}},
            openscreen::cast::Dimensions{1920, 1080, {60, 1}}, 300000,
            144000000, std::chrono::milliseconds(4000)}},
    openscreen::cast::DisplayDescription{
        openscreen::cast::Dimensions{1280, 720, {60, 1}},
        openscreen::cast::AspectRatio{16, 9},
        openscreen::cast::AspectRatioConstraint::kFixed,
    },
};

class MockRemotingSource : public media::mojom::RemotingSource {
 public:
  MockRemotingSource() = default;
  ~MockRemotingSource() override = default;

  void Bind(mojo::PendingReceiver<media::mojom::RemotingSource> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void reset_on_disconnect() {
    receiver_.set_disconnect_handler(
        base::BindOnce(&MockRemotingSource::reset, weak_factory_.GetWeakPtr()));
  }

  void reset() { receiver_.reset(); }

  MOCK_METHOD0(OnSinkGone, void());
  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD1(OnStartFailed, void(RemotingStartFailReason));
  MOCK_METHOD1(OnMessageFromSink, void(const std::vector<uint8_t>&));
  MOCK_METHOD1(OnStopped, void(RemotingStopReason));
  MOCK_METHOD1(OnSinkAvailable, void(const RemotingSinkMetadata&));
  void OnSinkAvailable(RemotingSinkMetadataPtr metadata) override {
    OnSinkAvailable(*metadata);
  }

 private:
  mojo::Receiver<media::mojom::RemotingSource> receiver_{this};
  base::WeakPtrFactory<MockRemotingSource> weak_factory_{this};
};

Json::Value ParseAsJsoncppValue(std::string_view document) {
  Json::CharReaderBuilder builder;
  Json::CharReaderBuilder::strictMode(&builder.settings_);
  EXPECT_FALSE(document.empty());

  Json::Value root_node;
  std::string error_msg;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  EXPECT_TRUE(reader->parse(&*document.begin(), &*document.end(), &root_node,
                            &error_msg));

  return root_node;
}

std::string Stringify(const Json::Value& value) {
  EXPECT_FALSE(value.empty());
  Json::StreamWriterBuilder factory;
  factory["indentation"] = "";

  std::unique_ptr<Json::StreamWriter> const writer(factory.newStreamWriter());
  std::ostringstream stream;
  writer->write(value, &stream);

  EXPECT_TRUE(stream);
  return stream.str();
}

openscreen::cast::SenderStats ConstructDefaultSenderStats() {
  return openscreen::cast::SenderStats{
      .audio_statistics = openscreen::cast::SenderStats::StatisticsList(),
      .audio_histograms = openscreen::cast::SenderStats::HistogramsList(),
      .video_statistics = openscreen::cast::SenderStats::StatisticsList(),
      .video_histograms = openscreen::cast::SenderStats::HistogramsList()};
}

}  // namespace

class OpenscreenSessionHostTest : public mojom::ResourceProvider,
                                  public mojom::SessionObserver,
                                  public mojom::CastMessageChannel,
                                  public ::testing::Test {
 public:
  OpenscreenSessionHostTest() = default;

  OpenscreenSessionHostTest(const OpenscreenSessionHostTest&) = delete;
  OpenscreenSessionHostTest& operator=(const OpenscreenSessionHostTest&) =
      delete;

  void TearDown() override {
    media::cast::encoding_support::ClearHardwareCodecDenyListForTesting();
  }

  ~OpenscreenSessionHostTest() override { task_environment_.RunUntilIdle(); }

 protected:
  // mojom::SessionObserver implementation.
  MOCK_METHOD(void, OnError, (SessionError));
  MOCK_METHOD(void, DidStart, ());
  MOCK_METHOD(void, DidStop, ());
  MOCK_METHOD(void, LogInfoMessage, (const std::string&));
  MOCK_METHOD(void, LogErrorMessage, (const std::string&));
  MOCK_METHOD(void, OnSourceChanged, ());
  MOCK_METHOD(void, OnRemotingStateChanged, (bool is_remoting));

  MOCK_METHOD(void, OnGetVideoCaptureHost, ());
  MOCK_METHOD(void, OnGetNetworkContext, ());
  MOCK_METHOD(void, OnCreateAudioStream, ());
  MOCK_METHOD(void, OnConnectToRemotingSource, ());

  // Called when an outbound message is sent.
  MOCK_METHOD(void, OnOutboundMessage, (SenderMessage::Type type));

  MOCK_METHOD(void, OnInitialized, ());

  // mojom::CastMessageChannel implementation (outbound messages).
  void OnMessage(mojom::CastMessagePtr message) override {
    EXPECT_TRUE(message->message_namespace == mojom::kWebRtcNamespace ||
                message->message_namespace == mojom::kRemotingNamespace);

    const Json::Value json_value =
        ParseAsJsoncppValue(message->json_format_data);

    ErrorOr<SenderMessage> parsed_message = SenderMessage::Parse(json_value);
    EXPECT_TRUE(parsed_message);
    last_sent_offer_ = parsed_message.value();
    if (parsed_message.value().type == SenderMessage::Type::kOffer) {
      EXPECT_GT(parsed_message.value().sequence_number, 0);
      const auto offer =
          absl::get<openscreen::cast::Offer>(parsed_message.value().body);

      for (const openscreen::cast::AudioStream& stream : offer.audio_streams) {
        EXPECT_EQ(
            base::Milliseconds(
                std::chrono::milliseconds(stream.stream.target_delay).count()),
            target_playout_delay_);
      }
      for (const openscreen::cast::VideoStream& stream : offer.video_streams) {
        EXPECT_EQ(
            base::Milliseconds(
                std::chrono::milliseconds(stream.stream.target_delay).count()),
            target_playout_delay_);
      }
    } else if (parsed_message.value().type ==
               SenderMessage::Type::kGetCapabilities) {
      EXPECT_GT(parsed_message.value().sequence_number, 0);
    }

    OnOutboundMessage(parsed_message.value().type);
  }

  // mojom::ResourceProvider overrides.
  void BindGpu(mojo::PendingReceiver<viz::mojom::Gpu> receiver) override {}
  void GetVideoCaptureHost(
      mojo::PendingReceiver<media::mojom::VideoCaptureHost> receiver) override {
    video_host_ =
        std::make_unique<NiceMock<FakeVideoCaptureHost>>(std::move(receiver));
    OnGetVideoCaptureHost();
  }

  void GetVideoEncoderMetricsProvider(
      mojo::PendingReceiver<media::mojom::VideoEncoderMetricsProvider> receiver)
      override {}

  void GetNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver) override {
    network_context_ =
        std::make_unique<NiceMock<MockNetworkContext>>(std::move(receiver));
    OnGetNetworkContext();
  }

  void CreateAudioStream(
      mojo::PendingRemote<mojom::AudioStreamCreatorClient> client,
      const media::AudioParameters& params,
      uint32_t total_segments) override {
    OnCreateAudioStream();
  }

  void ConnectToRemotingSource(
      mojo::PendingRemote<media::mojom::Remoter> remoter,
      mojo::PendingReceiver<media::mojom::RemotingSource> receiver) override {
    remoter_.Bind(std::move(remoter));
    remoter_.reset_on_disconnect();
    remoting_source_.Bind(std::move(receiver));
    remoting_source_.reset_on_disconnect();
    OnConnectToRemotingSource();
  }

  void GenerateAndReplyWithAnswer() {
    ASSERT_TRUE(session_host_);
    ASSERT_TRUE(last_sent_offer_);

    const openscreen::cast::Offer& offer =
        absl::get<openscreen::cast::Offer>(last_sent_offer_->body);
    openscreen::cast::Answer answer{.udp_port = 1234};

    if (!offer.audio_streams.empty()) {
      answer.send_indexes.push_back(offer.audio_streams[0].stream.index);
      answer.ssrcs.push_back(next_receiver_ssrc_++);
    }

    if (!offer.video_streams.empty()) {
      answer.send_indexes.push_back(offer.video_streams[0].stream.index);
      answer.ssrcs.push_back(next_receiver_ssrc_++);
    }

    openscreen::cast::ReceiverMessage receiver_message{
        .type = openscreen::cast::ReceiverMessage::Type::kAnswer,
        .sequence_number = last_sent_offer_->sequence_number,
        .valid = true,
        .body = std::move(answer)};
    Json::Value message_json = receiver_message.ToJson().value();
    ErrorOr<std::string> message_string = Stringify(message_json);
    ASSERT_TRUE(message_string);

    mojom::CastMessagePtr message = mojom::CastMessage::New(
        openscreen::cast::kCastWebrtcNamespace, message_string.value());
    inbound_channel_->OnMessage(std::move(message));
  }

  OpenscreenSessionHost::AsyncInitializedCallback MakeOnInitializedCallback() {
    return base::BindOnce(&OpenscreenSessionHostTest::OnInitialized,
                          base::Unretained(this));
  }

  // Create a mirroring session. Expect to send OFFER message.
  void CreateSession(SessionType session_type,
                     bool is_remote_playback = false,
                     bool enable_rtcp_reporting = false) {
    session_type_ = session_type;
    is_remote_playback_ = is_remote_playback;
    mojom::SessionParametersPtr session_params =
        mojom::SessionParameters::New();
    session_params->type = session_type_;
    session_params->receiver_address = receiver_endpoint_.address();
    session_params->receiver_model_name = "Chromecast";
    session_params->source_id = "sender-123";
    session_params->destination_id = "receiver-456";
    if (target_playout_delay_ != kDefaultPlayoutDelay) {
      session_params->target_playout_delay = target_playout_delay_;
    }
    if (force_letterboxing_) {
      session_params->force_letterboxing = true;
    }
    if (enable_rtcp_reporting) {
      session_params->enable_rtcp_reporting = true;
    }
    session_params->is_remote_playback = is_remote_playback_;
    cast_mode_ = "mirroring";
    mojo::PendingRemote<mojom::ResourceProvider> resource_provider_remote;
    mojo::PendingRemote<mojom::SessionObserver> session_observer_remote;
    mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel_remote;
    resource_provider_receiver_.Bind(
        resource_provider_remote.InitWithNewPipeAndPassReceiver());
    session_observer_receiver_.Bind(
        session_observer_remote.InitWithNewPipeAndPassReceiver());
    outbound_channel_receiver_.Bind(
        outbound_channel_remote.InitWithNewPipeAndPassReceiver());
    // Expect to send OFFER message when session is created.
    EXPECT_CALL(*this, OnGetNetworkContext());
    EXPECT_CALL(*this, OnError(_)).Times(0);
    EXPECT_CALL(*this, OnOutboundMessage(SenderMessage::Type::kOffer));
    EXPECT_CALL(*this, OnInitialized());
    EXPECT_CALL(*this, OnRemotingStateChanged(false));
    session_host_ = std::make_unique<OpenscreenSessionHost>(
        std::move(session_params), gfx::Size(1920, 1080),
        std::move(session_observer_remote), std::move(resource_provider_remote),
        std::move(outbound_channel_remote),
        inbound_channel_.BindNewPipeAndPassReceiver(), nullptr);
    session_host_->AsyncInitialize(MakeOnInitializedCallback());
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
  }

  // Negotiates a mirroring session.
  void StartSession() {
    ASSERT_EQ(cast_mode_, "mirroring");
    const int num_to_get_video_host =
        session_type_ == SessionType::AUDIO_ONLY ? 0 : 1;
    const int num_to_create_audio_stream =
        session_type_ == SessionType::VIDEO_ONLY ? 0 : 1;
    EXPECT_CALL(*this, OnGetVideoCaptureHost()).Times(num_to_get_video_host);
    EXPECT_CALL(*this, OnCreateAudioStream()).Times(num_to_create_audio_stream);
    EXPECT_CALL(*this, OnError(_)).Times(0);
    if (!is_remote_playback_) {
      EXPECT_CALL(*this,
                  OnOutboundMessage(SenderMessage::Type::kGetCapabilities));
    }
    EXPECT_CALL(*this, DidStart());
    GenerateAndReplyWithAnswer();
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
  }

  // Negotiate mirroring.
  void NegotiateMirroring() { session_host_->NegotiateMirroring(); }

  void StopSession() {
    if (video_host_)
      EXPECT_CALL(*video_host_, OnStopped());
    EXPECT_CALL(*this, DidStop());
    session_host_.reset();
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
  }

  void RemotePlaybackSessionTimeOut() {
    EXPECT_TRUE(video_host_);
    EXPECT_CALL(*video_host_, OnStopped());
    EXPECT_CALL(*this, DidStop());
    task_environment_.AdvanceClock(base::Seconds(5));
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
  }

  void CaptureOneVideoFrame() {
    ASSERT_EQ(cast_mode_, "mirroring");
    ASSERT_TRUE(video_host_);
    // Expect to send out some UDP packets.
    EXPECT_CALL(*network_context_->udp_socket(), OnSendTo()).Times(AtLeast(1));
    EXPECT_CALL(*video_host_, ReleaseBuffer(_, _, _));
    // Send one video frame to the consumer.
    video_host_->SendOneFrame(gfx::Size(64, 32), base::TimeTicks::Now());
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(network_context_.get());
    Mock::VerifyAndClear(video_host_.get());
  }

  void SignalAnswerTimeout() {
    EXPECT_CALL(*this, LogErrorMessage(_));
    if (cast_mode_ == "mirroring") {
      EXPECT_CALL(*this, DidStop());
      EXPECT_CALL(*this, OnError(SessionError::ANSWER_TIME_OUT));
    } else {
      EXPECT_CALL(*this, DidStop()).Times(0);
      EXPECT_CALL(*this, OnError(SessionError::ANSWER_TIME_OUT)).Times(0);
      // Expect to send OFFER message to fallback on mirroring.
      EXPECT_CALL(*this, OnOutboundMessage(SenderMessage::Type::kOffer));
      EXPECT_CALL(*this, OnRemotingStateChanged(false));
      // The start of remoting is expected to fail.
      EXPECT_CALL(
          remoting_source_,
          OnStartFailed(RemotingStartFailReason::INVALID_ANSWER_MESSAGE));
      EXPECT_CALL(remoting_source_, OnSinkGone()).Times(AtLeast(1));
    }

    session_host_->OnError(session_host_->session_.get(),
                           openscreen::Error::Code::kAnswerTimeout);
    task_environment_.RunUntilIdle();
    cast_mode_ = "mirroring";
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  void SendRemotingCapabilities() {
    static const openscreen::cast::RemotingCapabilities capabilities{
        {openscreen::cast::AudioCapability::kBaselineSet,
         openscreen::cast::AudioCapability::kAac,
         openscreen::cast::AudioCapability::kOpus},
        {openscreen::cast::VideoCapability::kSupports4k,
         openscreen::cast::VideoCapability::kVp8,
         openscreen::cast::VideoCapability::kVp9,
         openscreen::cast::VideoCapability::kH264,
         openscreen::cast::VideoCapability::kHevc}};

    EXPECT_CALL(*this, OnConnectToRemotingSource());
    EXPECT_CALL(remoting_source_, OnSinkAvailable(_));

    session_host_->OnCapabilitiesDetermined(session_host_->session_.get(),
                                            capabilities);
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  void SendRemotingNotSupported() {
    // Remoting not being supported should NOT surface an error.
    EXPECT_CALL(*this, OnError(_)).Times(0);

    session_host_->OnError(
        session_host_->session_.get(),
        openscreen::Error(openscreen::Error::Code::kRemotingNotSupported,
                          "this receiver does not support remoting"));

    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
  }

  void StartRemoting() {
    base::RunLoop run_loop;
    ASSERT_TRUE(remoter_.is_bound());
    // GET_CAPABILITIES is only sent once at the start of mirroring.
    EXPECT_CALL(*this, OnOutboundMessage(SenderMessage::Type::kGetCapabilities))
        .Times(0);
    EXPECT_CALL(*this, OnOutboundMessage(SenderMessage::Type::kOffer))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    EXPECT_CALL(*this, OnRemotingStateChanged(true));
    if (is_remote_playback_) {
      EXPECT_TRUE(video_host_ && video_host_->paused());
    }
    remoter_->Start();
    run_loop.Run();
    task_environment_.RunUntilIdle();
    cast_mode_ = "remoting";
    Mock::VerifyAndClear(this);
  }

  void RemotingStarted() {
    ASSERT_EQ(cast_mode_, "remoting");
    EXPECT_CALL(remoting_source_, OnStarted());
    GenerateAndReplyWithAnswer();
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  void StopRemotingAndRestartMirroring() {
    ASSERT_EQ(cast_mode_, "remoting");
    const RemotingStopReason reason = RemotingStopReason::LOCAL_PLAYBACK;
    // Expect to send OFFER message to fallback on mirroring.
    EXPECT_CALL(*this, OnOutboundMessage(SenderMessage::Type::kOffer));
    EXPECT_CALL(*this, OnRemotingStateChanged(false));
    EXPECT_CALL(remoting_source_, OnStopped(reason));
    remoter_->Stop(reason);
    task_environment_.RunUntilIdle();
    cast_mode_ = "mirroring";
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  void StopRemotingAndStopSession() {
    ASSERT_EQ(cast_mode_, "remoting");
    const RemotingStopReason reason = RemotingStopReason::LOCAL_PLAYBACK;
    EXPECT_CALL(remoting_source_, OnStopped(reason));
    if (video_host_)
      EXPECT_CALL(*video_host_, OnStopped());
    EXPECT_CALL(*this, DidStop());
    remoter_->Stop(reason);
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  void SwitchSourceTab() {
    const int get_video_host_call_count =
        session_type_ == SessionType::AUDIO_ONLY ? 0 : 1;
    const int create_audio_stream_call_count =
        session_type_ == SessionType::VIDEO_ONLY ? 0 : 1;
    EXPECT_CALL(*video_host_, OnStopped());
    EXPECT_CALL(*this, OnGetVideoCaptureHost())
        .Times(get_video_host_call_count);
    EXPECT_CALL(*this, OnCreateAudioStream())
        .Times(create_audio_stream_call_count);
    EXPECT_CALL(*this, OnConnectToRemotingSource());
    EXPECT_CALL(*this, OnSourceChanged());

    if (cast_mode_ == "remoting") {
      EXPECT_CALL(*this, OnOutboundMessage(SenderMessage::Type::kOffer));
      EXPECT_CALL(*this, OnError(_)).Times(0);
      // GET_CAPABILITIES is only sent once at the start of mirroring.
      EXPECT_CALL(*this,
                  OnOutboundMessage(SenderMessage::Type::kGetCapabilities))
          .Times(0);
      const RemotingStopReason reason = RemotingStopReason::LOCAL_PLAYBACK;
      EXPECT_CALL(remoting_source_, OnStopped(reason));
    }

    ASSERT_TRUE(session_host_);
    session_host_->SwitchSourceTab();
    task_environment_.RunUntilIdle();

    // Offer/Answer calls are unnecessary when switching from mirroring to
    // mirroring.
    if (cast_mode_ != "mirroring") {
      cast_mode_ = "mirroring";
      GenerateAndReplyWithAnswer();
      task_environment_.RunUntilIdle();
    }

    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  void SetTargetPlayoutDelay(int target_playout_delay_ms) {
    target_playout_delay_ = base::Milliseconds(target_playout_delay_ms);
  }

  void ForceLetterboxing() { force_letterboxing_ = true; }

  void SetAnswer(std::unique_ptr<openscreen::cast::Answer> answer) {
    answer_ = std::move(answer);
  }

  OpenscreenSessionHost& session_host() { return *session_host_; }

  const openscreen::cast::SenderMessage& last_sent_offer() const {
    EXPECT_TRUE(last_sent_offer_);
    return *last_sent_offer_;
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  void PushEncoderStatusChange(const media::cast::FrameSenderConfig& config,
                               media::cast::OperationalStatus status) {
    session_host_->OnEncoderStatusChange(config, status);
  }

  const std::vector<media::cast::FrameSenderConfig>& LastOfferedVideoConfigs() {
    return session_host_->last_offered_video_configs_;
  }

  void SetSupportedProfiles(
      media::VideoEncodeAccelerator::SupportedProfiles profiles) {
    session_host_->supported_profiles_ = std::move(profiles);
  }

 protected:
  std::unique_ptr<FakeVideoCaptureHost> video_host_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  const net::IPEndPoint receiver_endpoint_ = GetFreeLocalPort();
  mojo::Receiver<mojom::ResourceProvider> resource_provider_receiver_{this};
  mojo::Receiver<mojom::SessionObserver> session_observer_receiver_{this};
  mojo::Receiver<mojom::CastMessageChannel> outbound_channel_receiver_{this};
  mojo::Remote<mojom::CastMessageChannel> inbound_channel_;
  SessionType session_type_ = SessionType::AUDIO_AND_VIDEO;
  bool is_remote_playback_ = false;
  mojo::Remote<media::mojom::Remoter> remoter_;
  NiceMock<MockRemotingSource> remoting_source_;
  std::string cast_mode_;
  base::TimeDelta target_playout_delay_{kDefaultPlayoutDelay};
  bool force_letterboxing_{false};

  std::unique_ptr<OpenscreenSessionHost> session_host_;
  std::unique_ptr<MockNetworkContext> network_context_;
  std::unique_ptr<openscreen::cast::Answer> answer_;

  int next_receiver_ssrc_{35336};
  std::optional<openscreen::cast::SenderMessage> last_sent_offer_;
};

TEST_F(OpenscreenSessionHostTest, AudioOnlyMirroring) {
  CreateSession(SessionType::AUDIO_ONLY);
  StartSession();
  StopSession();
}

TEST_F(OpenscreenSessionHostTest, VideoOnlyMirroring) {
  SetTargetPlayoutDelay(1000);
  CreateSession(SessionType::VIDEO_ONLY);
  StartSession();
  CaptureOneVideoFrame();
  StopSession();
}

TEST_F(OpenscreenSessionHostTest, AudioAndVideoMirroring) {
  SetTargetPlayoutDelay(150);
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  StopSession();
}

// TODO(crbug.com/1363512): Remove support for sender side letterboxing.
TEST_F(OpenscreenSessionHostTest, AnswerWithConstraints) {
  SetAnswer(std::make_unique<openscreen::cast::Answer>(kAnswerWithConstraints));
  media::VideoCaptureParams::SuggestedConstraints expected_constraints = {
      .min_frame_size = gfx::Size(320, 180),
      .max_frame_size = gfx::Size(1920, 1080),
      .fixed_aspect_ratio = true};
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  StopSession();
  EXPECT_EQ(video_host_->GetVideoCaptureParams().SuggestConstraints(),
            expected_constraints);
}

TEST_F(OpenscreenSessionHostTest, AnswerWithConstraintsLetterboxDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCastDisableLetterboxing);
  SetAnswer(std::make_unique<openscreen::cast::Answer>(kAnswerWithConstraints));
  media::VideoCaptureParams::SuggestedConstraints expected_constraints = {
      .min_frame_size = gfx::Size(2, 2),
      .max_frame_size = gfx::Size(1920, 1080),
      .fixed_aspect_ratio = false};
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  StopSession();
  EXPECT_EQ(video_host_->GetVideoCaptureParams().SuggestConstraints(),
            expected_constraints);
}

// TODO(crbug.com/1363512): Remove support for sender side letterboxing.
TEST_F(OpenscreenSessionHostTest, AnswerWithConstraintsLetterboxForced) {
  ForceLetterboxing();
  SetAnswer(std::make_unique<openscreen::cast::Answer>(kAnswerWithConstraints));
  media::VideoCaptureParams::SuggestedConstraints expected_constraints = {
      .min_frame_size = gfx::Size(320, 180),
      .max_frame_size = gfx::Size(1920, 1080),
      .fixed_aspect_ratio = true};
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  StopSession();
  EXPECT_EQ(video_host_->GetVideoCaptureParams().SuggestConstraints(),
            expected_constraints);
}

TEST_F(OpenscreenSessionHostTest, AnswerTimeout) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  SignalAnswerTimeout();
}

TEST_F(OpenscreenSessionHostTest, SwitchToAndFromRemoting) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  SendRemotingCapabilities();
  StartRemoting();
  RemotingStarted();
  StopRemotingAndRestartMirroring();
  StopSession();
}

TEST_F(OpenscreenSessionHostTest, SwitchFromRemotingForRemotePlayback) {
  CreateSession(SessionType::AUDIO_AND_VIDEO, true);
  StartSession();
  StartRemoting();
  RemotingStarted();
  StopRemotingAndStopSession();
}

TEST_F(OpenscreenSessionHostTest, RemotingNotSupported) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  SendRemotingNotSupported();
  StopSession();
}

TEST_F(OpenscreenSessionHostTest, StopSessionWhileRemoting) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  SendRemotingCapabilities();
  StartRemoting();
  RemotingStarted();
  StopSession();
}

TEST_F(OpenscreenSessionHostTest, StartRemotingFailed) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  SendRemotingCapabilities();
  StartRemoting();
  SignalAnswerTimeout();
  GenerateAndReplyWithAnswer();
  CaptureOneVideoFrame();
  StopSession();
}

TEST_F(OpenscreenSessionHostTest, SwitchSourceTabFromMirroring) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  SendRemotingCapabilities();
  SwitchSourceTab();
  StartRemoting();
  RemotingStarted();
  StopSession();
}

TEST_F(OpenscreenSessionHostTest, SwitchSourceTabFromRemoting) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  SendRemotingCapabilities();
  StartRemoting();
  RemotingStarted();
  SwitchSourceTab();
  StopSession();
}

TEST_F(OpenscreenSessionHostTest, StartRemotePlaybackTimeOut) {
  CreateSession(SessionType::AUDIO_AND_VIDEO, true);
  StartSession();
  RemotePlaybackSessionTimeOut();
}

// TODO(crbug.com/40238532): reenable adaptive playout delay.
TEST_F(OpenscreenSessionHostTest, ChangeTargetPlayoutDelay) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();

  // Currently new delays are ignored due to the playout delay being bounded by
  // the minimum and maximum both being set to the default value.
  session_host().SetTargetPlayoutDelay(base::Milliseconds(300));
  EXPECT_EQ(session_host().audio_stream_->GetTargetPlayoutDelay(),
            kDefaultPlayoutDelay);
  EXPECT_EQ(session_host().audio_stream_->GetTargetPlayoutDelay(),
            kDefaultPlayoutDelay);

  StopSession();
}

TEST_F(OpenscreenSessionHostTest, UpdateBandwidthEstimate) {
  CreateSession(SessionType::VIDEO_ONLY);
  StartSession();

  constexpr int kMinVideoBitrate = 393216;
  constexpr int kMaxVideoBitrate = 1250000;
  // Default bitrate should be twice the minimum.
  EXPECT_EQ(786432, session_host().GetSuggestedVideoBitrate(kMinVideoBitrate,
                                                            kMaxVideoBitrate));

  // If the estimate is below the minimum, it should stay at the minimum.
  session_host().forced_bandwidth_estimate_for_testing_ = 1000;
  session_host().UpdateBandwidthEstimate();
  EXPECT_EQ(kMinVideoBitrate, session_host().GetSuggestedVideoBitrate(
                                  kMinVideoBitrate, kMaxVideoBitrate));

  // It should gradually reach the max bandwidth estimate when raised.
  session_host().forced_bandwidth_estimate_for_testing_ = 1000000;
  session_host().UpdateBandwidthEstimate();
  EXPECT_EQ(432537, session_host().GetSuggestedVideoBitrate(kMinVideoBitrate,
                                                            kMaxVideoBitrate));

  session_host().UpdateBandwidthEstimate();
  EXPECT_EQ(475790, session_host().GetSuggestedVideoBitrate(kMinVideoBitrate,
                                                            kMaxVideoBitrate));
  for (int i = 0; i < 20; ++i) {
    session_host().UpdateBandwidthEstimate();
  }
  // The max should be 80% of `forced_bandwidth_estimate_for_testing_`.
  EXPECT_EQ(800000, session_host().GetSuggestedVideoBitrate(kMinVideoBitrate,
                                                            kMaxVideoBitrate));

  // The video bitrate should stay saturated at the cap when reached.
  session_host().forced_bandwidth_estimate_for_testing_ = kMaxVideoBitrate + 1;
  for (int i = 0; i < 20; ++i) {
    session_host().UpdateBandwidthEstimate();
  }
  // The max should be 80% of `kMaxVideoBitrate`.
  EXPECT_EQ(1000000, session_host().GetSuggestedVideoBitrate(kMinVideoBitrate,
                                                             kMaxVideoBitrate));

  StopSession();
}

TEST_F(OpenscreenSessionHostTest, CanRequestRefresh) {
  CreateSession(SessionType::VIDEO_ONLY);

  // We just want to make sure this doesn't result in an error or crash.
  session_host().RequestRefreshFrame();
}

TEST_F(OpenscreenSessionHostTest, Vp9CodecEnabledInOffer) {
  base::test::ScopedFeatureList feature_list(media::kCastStreamingVp9);
  CreateSession(SessionType::VIDEO_ONLY);

  const openscreen::cast::Offer& offer =
      absl::get<openscreen::cast::Offer>(last_sent_offer().body);

  // We should have offered VP9.
  EXPECT_TRUE(
      std::any_of(offer.video_streams.begin(), offer.video_streams.end(),
                  [](const openscreen::cast::VideoStream& stream) {
                    return stream.codec == openscreen::cast::VideoCodec::kVp9;
                  }));
}

TEST_F(OpenscreenSessionHostTest, Av1CodecEnabledInOffer) {
// Cast streaming of AV1 is desktop only.
#if !BUILDFLAG(IS_ANDROID) && defined(ENABLE_LIBAOM)
  base::test::ScopedFeatureList feature_list(media::kCastStreamingAv1);
  CreateSession(SessionType::VIDEO_ONLY);

  const openscreen::cast::Offer& offer =
      absl::get<openscreen::cast::Offer>(last_sent_offer().body);

  // We should have offered AV1.
  EXPECT_TRUE(
      std::any_of(offer.video_streams.begin(), offer.video_streams.end(),
                  [](const openscreen::cast::VideoStream& stream) {
                    return stream.codec == openscreen::cast::VideoCodec::kAv1;
                  }));
#endif
}

TEST_F(OpenscreenSessionHostTest, ShouldEnableHardwareVp8EncodingIfSupported) {
  CreateSession(SessionType::VIDEO_ONLY);

  // Mock the profiles to enable VP8 hardware encode.
  SetSupportedProfiles(
      std::vector<media::VideoEncodeAccelerator::SupportedProfile>{
          media::VideoEncodeAccelerator::SupportedProfile(
              media::VideoCodecProfile::VP8PROFILE_ANY,
              gfx::Size{1920, 1080})});
  NegotiateMirroring();
  task_environment().RunUntilIdle();

  const openscreen::cast::Offer& offer =
      absl::get<openscreen::cast::Offer>(last_sent_offer().body);

  // We should have offered VP8.
  EXPECT_TRUE(
      std::any_of(offer.video_streams.begin(), offer.video_streams.end(),
                  [](const openscreen::cast::VideoStream& stream) {
                    return stream.codec == openscreen::cast::VideoCodec::kVp8;
                  }));

  // We should have put a video config for VP8 with hardware enabled in the last
  // offered configs.
  EXPECT_TRUE(std::any_of(LastOfferedVideoConfigs().begin(),
                          LastOfferedVideoConfigs().end(),

                          [](const media::cast::FrameSenderConfig& config) {
                            return config.video_codec() ==
                                       media::VideoCodec::kVP8 &&
                                   config.use_hardware_encoder;
                          }));
}

TEST_F(OpenscreenSessionHostTest,
       ShouldDisableHardwareEncodingIfEncoderReportsAnIssue) {
  CreateSession(SessionType::VIDEO_ONLY);

  // Mock the profiles to enable VP8 hardware encode.
  SetSupportedProfiles(
      std::vector<media::VideoEncodeAccelerator::SupportedProfile>{
          media::VideoEncodeAccelerator::SupportedProfile(
              media::VideoCodecProfile::VP8PROFILE_ANY,
              gfx::Size{1920, 1080})});
  NegotiateMirroring();
  task_environment().RunUntilIdle();

  const openscreen::cast::Offer& offer =
      absl::get<openscreen::cast::Offer>(last_sent_offer().body);

  // We should have offered VP8.
  EXPECT_TRUE(
      std::any_of(offer.video_streams.begin(), offer.video_streams.end(),
                  [](const openscreen::cast::VideoStream& stream) {
                    return stream.codec == openscreen::cast::VideoCodec::kVp8;
                  }));

  // We should have put a video config for VP8 with hardware enabled in the last
  // offered configs.
  EXPECT_TRUE(std::any_of(
      LastOfferedVideoConfigs().begin(), LastOfferedVideoConfigs().end(),
      [](const media::cast::FrameSenderConfig& config) {
        return config.video_codec() == media::VideoCodec::kVP8 &&
               config.use_hardware_encoder;
      }));

  // Oh no! The encoder had a problem.
  FrameSenderConfig config;
  config.use_hardware_encoder = true;
  config.rtp_payload_type = media::cast::RtpPayloadType::VIDEO_VP8;
  config.video_codec_params =
      media::cast::VideoCodecParams{media::VideoCodec::kVP8};
  PushEncoderStatusChange(
      config, media::cast::OperationalStatus::STATUS_CODEC_INIT_FAILED);

  // This should have forced a renegotiation.
  const openscreen::cast::Offer& second_offer =
      absl::get<openscreen::cast::Offer>(last_sent_offer().body);

  // We should have offered VP8 again.
  EXPECT_TRUE(std::any_of(
      second_offer.video_streams.begin(), second_offer.video_streams.end(),
      [](const openscreen::cast::VideoStream& stream) {
        return stream.codec == openscreen::cast::VideoCodec::kVp8;
      }));

  // We should have put a video config for VP8 with hardware DISABLED in the
  // last offered configs.
  EXPECT_TRUE(std::any_of(
      LastOfferedVideoConfigs().begin(), LastOfferedVideoConfigs().end(),

      [](const media::cast::FrameSenderConfig& config) {
        return config.video_codec() == media::VideoCodec::kVP8 &&
               !config.use_hardware_encoder;
      }));
}

TEST_F(OpenscreenSessionHostTest, ShouldEnableHardwareH264EncodingIfSupported) {
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_CHROMEOS)
  CreateSession(SessionType::VIDEO_ONLY);

  SetSupportedProfiles(
      std::vector<media::VideoEncodeAccelerator::SupportedProfile>{
          media::VideoEncodeAccelerator::SupportedProfile(
              media::VideoCodecProfile::H264PROFILE_MIN,
              gfx::Size{1920, 1080})});
  NegotiateMirroring();
  task_environment().RunUntilIdle();

  const openscreen::cast::Offer& offer =
      absl::get<openscreen::cast::Offer>(last_sent_offer().body);

  // We should have offered H264.
  EXPECT_TRUE(
      std::any_of(offer.video_streams.begin(), offer.video_streams.end(),
                  [](const openscreen::cast::VideoStream& stream) {
                    return stream.codec == openscreen::cast::VideoCodec::kH264;
                  }));

  // We should have put a video config for H264 with hardware enabled in the
  // last offered configs.
  EXPECT_TRUE(std::any_of(LastOfferedVideoConfigs().begin(),
                          LastOfferedVideoConfigs().end(),

                          [](const media::cast::FrameSenderConfig& config) {
                            return config.video_codec() ==
                                       media::VideoCodec::kH264 &&
                                   config.use_hardware_encoder;
                          }));
#endif
}

TEST_F(OpenscreenSessionHostTest, GetStatsDefault) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  EXPECT_TRUE(session_host().GetMirroringStats().empty());
}

TEST_F(OpenscreenSessionHostTest, GetStatsEnabled) {
  CreateSession(SessionType::AUDIO_AND_VIDEO, /* remote_playback */ false,
                /* rtcp_reporting */ true);
  session_host().SetSenderStatsForTest(ConstructDefaultSenderStats());
  EXPECT_FALSE(session_host().GetMirroringStats().empty());
}

}  // namespace mirroring
