// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/session.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/mirroring/service/fake_network_service.h"
#include "components/mirroring/service/fake_video_capture_host.h"
#include "components/mirroring/service/mirror_settings.h"
#include "components/mirroring/service/mirroring_features.h"
#include "components/mirroring/service/receiver_response.h"
#include "components/mirroring/service/value_util.h"
#include "media/capture/video_capture_types.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/net_utility.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/viz/public/cpp/gpu/gpu.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/ssrc.h"
#include "ui/gfx/geometry/size.h"

using media::cast::FrameSenderConfig;
using media::cast::Packet;
using media::mojom::RemotingSinkMetadata;
using media::mojom::RemotingSinkMetadataPtr;
using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingStopReason;
using mirroring::mojom::SessionError;
using mirroring::mojom::SessionType;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::SaveArg;

namespace mirroring {

namespace {

constexpr int kDefaultPlayoutDelay = 400;  // ms

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
  MockRemotingSource() {}
  ~MockRemotingSource() override {}

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

}  // namespace

class SessionTest : public mojom::ResourceProvider,
                    public mojom::SessionObserver,
                    public mojom::CastMessageChannel,
                    public ::testing::Test {
 public:
  SessionTest() = default;

  SessionTest(const SessionTest&) = delete;
  SessionTest& operator=(const SessionTest&) = delete;

  ~SessionTest() override { task_environment_.RunUntilIdle(); }

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

  MOCK_METHOD(void, OnOutboundMessage, (const std::string& message_type));

  MOCK_METHOD(void, OnInitDone, ());

  // mojom::CastMessageChannel implementation (outbound messages).
  void OnMessage(mojom::CastMessagePtr message) override {
    EXPECT_TRUE(message->message_namespace == mojom::kWebRtcNamespace ||
                message->message_namespace == mojom::kRemotingNamespace);
    absl::optional<base::Value> value =
        base::JSONReader::Read(message->json_format_data);
    ASSERT_TRUE(value);
    std::string message_type;
    EXPECT_TRUE(GetString(*value, "type", &message_type));
    if (message_type == "OFFER") {
      EXPECT_TRUE(GetInt(*value, "seqNum", &offer_sequence_number_));
      base::Value::Dict* offer = value->GetDict().FindDict("offer");
      ASSERT_TRUE(offer);
      base::Value* raw_streams = offer->Find("supportedStreams");
      if (raw_streams) {
        for (auto& list_value : raw_streams->GetList()) {
          EXPECT_EQ(*list_value.GetDict().FindInt("targetDelay"),
                    target_playout_delay_ms_);
        }
      }
    } else if (message_type == "GET_CAPABILITIES") {
      EXPECT_TRUE(GetInt(*value, "seqNum", &capability_sequence_number_));
    }
    OnOutboundMessage(message_type);
  }

  // mojom::ResourceProvider implemenation.
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

  void ForceLetterboxing() { force_letterboxing_ = true; }

  void SendAnswer() {
    ASSERT_TRUE(session_);
    std::vector<FrameSenderConfig> audio_configs;
    std::vector<FrameSenderConfig> video_configs;
    if (session_type_ != SessionType::VIDEO_ONLY) {
      if (cast_mode_ == "remoting") {
        audio_configs.emplace_back(MirrorSettings::GetDefaultAudioConfig(
            media::cast::RtpPayloadType::REMOTE_AUDIO,
            media::cast::Codec::kAudioRemote));
      } else {
        EXPECT_EQ("mirroring", cast_mode_);
        audio_configs.emplace_back(MirrorSettings::GetDefaultAudioConfig(
            media::cast::RtpPayloadType::AUDIO_OPUS,
            media::cast::Codec::kAudioOpus));
      }
    }
    if (session_type_ != SessionType::AUDIO_ONLY) {
      if (cast_mode_ == "remoting") {
        video_configs.emplace_back(MirrorSettings::GetDefaultVideoConfig(
            media::cast::RtpPayloadType::REMOTE_VIDEO,
            media::cast::Codec::kVideoRemote));
      } else {
        EXPECT_EQ("mirroring", cast_mode_);
        video_configs.emplace_back(MirrorSettings::GetDefaultVideoConfig(
            media::cast::RtpPayloadType::VIDEO_VP8,
            media::cast::Codec::kVideoVp8));
      }
    }

    std::unique_ptr<openscreen::cast::Answer> answer;
    if (answer_) {
      answer.swap(answer_);
    } else {
      answer = std::make_unique<openscreen::cast::Answer>();
    }

    answer->udp_port = receiver_endpoint_.port();
    const int number_of_configs = audio_configs.size() + video_configs.size();
    for (int i = 0; i < number_of_configs; ++i) {
      answer->send_indexes.push_back(i);
      answer->ssrcs.push_back(31 + i);  // Arbitrary receiver SSRCs.
    }

    auto response = ReceiverResponse::CreateAnswerResponseForTesting(
        offer_sequence_number_, std::move(answer));
    session_->OnAnswer(audio_configs, video_configs, response);
    task_environment_.RunUntilIdle();
  }

  Session::AsyncInitializeDoneCB MakeInitDoneCB() {
    return base::BindOnce(&SessionTest::OnInitDone, base::Unretained(this));
  }

  // Create a mirroring session. Expect to send OFFER message.
  void CreateSession(SessionType session_type,
                     bool is_remote_playback = false,
                     bool enable_rtcp_reporting = false) {
    session_type_ = session_type;
    is_remote_playback_ = is_remote_playback;
    mojom::SessionParametersPtr session_params =
        mojom::SessionParameters::New();
    session_params->receiver_address = receiver_endpoint_.address();
    session_params->type = session_type_;
    session_params->receiver_model_name = "Chromecast";
    if (target_playout_delay_ms_ != kDefaultPlayoutDelay) {
      session_params->target_playout_delay =
          base::Milliseconds(target_playout_delay_ms_);
    }
    if (force_letterboxing_) {
      session_params->force_letterboxing = true;
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
    EXPECT_CALL(*this, OnGetNetworkContext()).Times(1);
    EXPECT_CALL(*this, OnError(_)).Times(0);
    EXPECT_CALL(*this, OnOutboundMessage("OFFER")).Times(1);
    EXPECT_CALL(*this, OnRemotingStateChanged(false)).Times(1);
    EXPECT_CALL(*this, OnInitDone()).Times(1);

    session_ = std::make_unique<Session>(
        std::move(session_params), gfx::Size(1920, 1080),
        std::move(session_observer_remote), std::move(resource_provider_remote),
        std::move(outbound_channel_remote),
        inbound_channel_.BindNewPipeAndPassReceiver(), nullptr);
    session_->AsyncInitialize(MakeInitDoneCB());
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
  }

  // Starts the mirroring session.
  void StartSession() {
    ASSERT_EQ(cast_mode_, "mirroring");
    // Except mirroing session starts after receiving ANSWER message.
    const int num_to_get_video_host =
        session_type_ == SessionType::AUDIO_ONLY ? 0 : 1;
    const int num_to_create_audio_stream =
        session_type_ == SessionType::VIDEO_ONLY ? 0 : 1;
    EXPECT_CALL(*this, OnGetVideoCaptureHost()).Times(num_to_get_video_host);
    EXPECT_CALL(*this, OnCreateAudioStream()).Times(num_to_create_audio_stream);
    EXPECT_CALL(*this, OnError(_)).Times(0);
    if (!is_remote_playback_) {
      EXPECT_CALL(*this, OnOutboundMessage("GET_CAPABILITIES")).Times(1);
    }
    EXPECT_CALL(*this, DidStart()).Times(1);
    SendAnswer();
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
  }

  void StopSession() {
    if (video_host_)
      EXPECT_CALL(*video_host_, OnStopped()).Times(1);
    EXPECT_CALL(*this, DidStop()).Times(1);
    session_.reset();
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
    EXPECT_CALL(*network_context_->udp_socket(), OnSend()).Times(AtLeast(1));
    EXPECT_CALL(*video_host_, ReleaseBuffer(_, _, _)).Times(1);
    // Send one video frame to the consumer.
    video_host_->SendOneFrame(gfx::Size(64, 32), base::TimeTicks::Now());
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(network_context_.get());
    Mock::VerifyAndClear(video_host_.get());
  }

  void SignalAnswerTimeout() {
    if (cast_mode_ == "mirroring") {
      EXPECT_CALL(*this, DidStop()).Times(1);
      EXPECT_CALL(*this, OnError(SessionError::ANSWER_TIME_OUT)).Times(1);
    } else {
      EXPECT_CALL(*this, DidStop()).Times(0);
      EXPECT_CALL(*this, OnError(SessionError::ANSWER_TIME_OUT)).Times(0);
      // Expect to send OFFER message to fallback on mirroring.
      EXPECT_CALL(*this, OnOutboundMessage("OFFER")).Times(1);
      EXPECT_CALL(*this, OnRemotingStateChanged(false)).Times(1);
      // The start of remoting is expected to fail.
      EXPECT_CALL(remoting_source_,
                  OnStartFailed(RemotingStartFailReason::INVALID_ANSWER_MESSAGE))
          .Times(1);
      EXPECT_CALL(remoting_source_, OnSinkGone()).Times(AtLeast(1));
    }

    session_->OnAnswer(std::vector<FrameSenderConfig>(),
                       std::vector<FrameSenderConfig>(), ReceiverResponse());
    task_environment_.RunUntilIdle();
    cast_mode_ = "mirroring";
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  void SendRemotingCapabilities() {
    EXPECT_CALL(*this, OnConnectToRemotingSource()).Times(1);
    EXPECT_CALL(remoting_source_, OnSinkAvailable(_)).Times(1);
    auto capabilities = std::make_unique<ReceiverCapability>();
    capabilities->remoting = 2;
    capabilities->media_caps =
        std::vector<std::string>({"video", "audio", "vp8", "opus"});
    auto response = ReceiverResponse::CreateCapabilitiesResponseForTesting(
        capability_sequence_number_, std::move(capabilities));
    session_->OnCapabilitiesResponse(response);
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  void StartRemoting() {
    base::RunLoop run_loop;
    ASSERT_TRUE(remoter_.is_bound());
    // GET_CAPABILITIES is only sent once at the start of mirroring.
    EXPECT_CALL(*this, OnOutboundMessage("GET_CAPABILITIES")).Times(0);
    EXPECT_CALL(*this, OnOutboundMessage("OFFER"))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    EXPECT_CALL(*this, OnRemotingStateChanged(true)).Times(1);
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
    EXPECT_CALL(remoting_source_, OnStarted()).Times(1);
    SendAnswer();
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  void StopRemotingAndRestartMirroring() {
    ASSERT_EQ(cast_mode_, "remoting");
    const RemotingStopReason reason = RemotingStopReason::LOCAL_PLAYBACK;
    // Expect to send OFFER message to fallback on mirroring.
    EXPECT_CALL(*this, OnOutboundMessage("OFFER")).Times(1);
    EXPECT_CALL(remoting_source_, OnStopped(reason)).Times(1);
    EXPECT_CALL(*this, OnRemotingStateChanged(false)).Times(1);
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
      EXPECT_CALL(*this, OnOutboundMessage("OFFER"));
      EXPECT_CALL(*this, OnError(_)).Times(0);
      // GET_CAPABILITIES is only sent once at the start of mirroring.
      EXPECT_CALL(*this, OnOutboundMessage("GET_CAPABILITIES")).Times(0);
      const RemotingStopReason reason = RemotingStopReason::LOCAL_PLAYBACK;
      EXPECT_CALL(remoting_source_, OnStopped(reason));
    }

    ASSERT_TRUE(session_);
    session_->SwitchSourceTab();
    task_environment_.RunUntilIdle();

    // Offer/Answer calls are unnecessary when switching from mirroring to
    // mirroring.
    if (cast_mode_ != "mirroring") {
      cast_mode_ = "mirroring";
      SendAnswer();
      task_environment_.RunUntilIdle();
    }

    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  void SetTargetPlayoutDelay(int target_playout_delay_ms) {
    target_playout_delay_ms_ = target_playout_delay_ms;
  }

  void SetAnswer(std::unique_ptr<openscreen::cast::Answer> answer) {
    answer_ = std::move(answer);
  }

  base::Value::Dict GetStats() { return session_->GetMirroringStats(); }

 protected:
  std::unique_ptr<FakeVideoCaptureHost> video_host_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  const net::IPEndPoint receiver_endpoint_ =
      media::cast::test::GetFreeLocalPort();
  mojo::Receiver<mojom::ResourceProvider> resource_provider_receiver_{this};
  mojo::Receiver<mojom::SessionObserver> session_observer_receiver_{this};
  mojo::Receiver<mojom::CastMessageChannel> outbound_channel_receiver_{this};
  mojo::Remote<mojom::CastMessageChannel> inbound_channel_;
  SessionType session_type_ = SessionType::AUDIO_AND_VIDEO;
  bool is_remote_playback_ = false;
  mojo::Remote<media::mojom::Remoter> remoter_;
  NiceMock<MockRemotingSource> remoting_source_;
  std::string cast_mode_;
  int32_t offer_sequence_number_ = -1;
  int32_t capability_sequence_number_ = -1;
  int32_t target_playout_delay_ms_ = kDefaultPlayoutDelay;
  bool force_letterboxing_{false};

  std::unique_ptr<Session> session_;
  std::unique_ptr<MockNetworkContext> network_context_;
  std::unique_ptr<openscreen::cast::Answer> answer_;
};

TEST_F(SessionTest, AudioOnlyMirroring) {
  CreateSession(SessionType::AUDIO_ONLY);
  StartSession();
  StopSession();
}

TEST_F(SessionTest, VideoOnlyMirroring) {
  SetTargetPlayoutDelay(1000);
  CreateSession(SessionType::VIDEO_ONLY);
  StartSession();
  CaptureOneVideoFrame();
  StopSession();
}

TEST_F(SessionTest, AudioAndVideoMirroring) {
  SetTargetPlayoutDelay(150);
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  StopSession();
}

// TODO(crbug.com/1363512): Remove support for sender side letterboxing.
TEST_F(SessionTest, AnswerWithConstraintsLetterboxEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kCastDisableLetterboxing);
  SetAnswer(std::make_unique<openscreen::cast::Answer>(kAnswerWithConstraints));
  media::VideoCaptureParams::SuggestedConstraints expected_constraints = {
      .min_frame_size = gfx::Size(320, 180),
      .max_frame_size = gfx::Size(1280, 720),
      .fixed_aspect_ratio = true};
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  StopSession();
  EXPECT_EQ(video_host_->GetVideoCaptureParams().SuggestConstraints(),
            expected_constraints);
}

TEST_F(SessionTest, AnswerWithConstraintsLetterboxDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCastDisableLetterboxing);
  SetAnswer(std::make_unique<openscreen::cast::Answer>(kAnswerWithConstraints));
  media::VideoCaptureParams::SuggestedConstraints expected_constraints = {
      .min_frame_size = gfx::Size(2, 2),
      .max_frame_size = gfx::Size(1280, 720),
      .fixed_aspect_ratio = false};
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  StopSession();
  EXPECT_EQ(video_host_->GetVideoCaptureParams().SuggestConstraints(),
            expected_constraints);
}

// TODO(crbug.com/1363512): Remove support for sender side letterboxing.
TEST_F(SessionTest, AnswerWithConstraintsLetterboxForced) {
  ForceLetterboxing();
  SetAnswer(std::make_unique<openscreen::cast::Answer>(kAnswerWithConstraints));
  media::VideoCaptureParams::SuggestedConstraints expected_constraints = {
      .min_frame_size = gfx::Size(320, 180),
      .max_frame_size = gfx::Size(1280, 720),
      .fixed_aspect_ratio = true};
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  StopSession();
  EXPECT_EQ(video_host_->GetVideoCaptureParams().SuggestConstraints(),
            expected_constraints);
}

TEST_F(SessionTest, AnswerTimeout) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  SignalAnswerTimeout();
}

TEST_F(SessionTest, SwitchToAndFromRemoting) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  SendRemotingCapabilities();
  StartRemoting();
  RemotingStarted();
  StopRemotingAndRestartMirroring();
  StopSession();
}

TEST_F(SessionTest, SwitchFromRemotingForRemotePlayback) {
  CreateSession(SessionType::AUDIO_AND_VIDEO, true);
  StartSession();
  StartRemoting();
  RemotingStarted();
  StopRemotingAndStopSession();
}

TEST_F(SessionTest, StopSessionWhileRemoting) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  SendRemotingCapabilities();
  StartRemoting();
  RemotingStarted();
  StopSession();
}

TEST_F(SessionTest, StartRemotingFailed) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  SendRemotingCapabilities();
  StartRemoting();
  SignalAnswerTimeout();
  // Resume mirroring.
  SendAnswer();
  CaptureOneVideoFrame();
  StopSession();
}

TEST_F(SessionTest, SwitchSourceTabFromMirroring) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  SendRemotingCapabilities();
  SwitchSourceTab();
  StartRemoting();
  RemotingStarted();
  StopSession();
}

TEST_F(SessionTest, SwitchSourceTabFromRemoting) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  SendRemotingCapabilities();
  StartRemoting();
  RemotingStarted();
  SwitchSourceTab();
  StopSession();
}

TEST_F(SessionTest, StartRemotePlaybackTimeOut) {
  CreateSession(SessionType::AUDIO_AND_VIDEO, true);
  StartSession();
  RemotePlaybackSessionTimeOut();
}

TEST_F(SessionTest, GetMirroringStatsDisabled) {
  SetTargetPlayoutDelay(150);
  CreateSession(SessionType::AUDIO_AND_VIDEO, false,
                false /* enable_rtcp_reporting */);
  StartSession();

  // By default, if there is no session logger we should return an empty dict.
  EXPECT_EQ(GetStats(), base::Value::Dict());
}

TEST_F(SessionTest, GetMirroringStatsEnabled) {
  SetTargetPlayoutDelay(150);
  CreateSession(SessionType::AUDIO_AND_VIDEO, false,
                true /* enable_rtcp_reporting */);
  StartSession();
  // Since no streaming data is mocked or sent in this test, an empty dict is
  // still returned.
  EXPECT_EQ(GetStats(), base::Value::Dict());
}

}  // namespace mirroring
