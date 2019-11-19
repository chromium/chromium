// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/session.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/mirroring/service/fake_network_service.h"
#include "components/mirroring/service/fake_video_capture_host.h"
#include "components/mirroring/service/mirror_settings.h"
#include "components/mirroring/service/receiver_response.h"
#include "components/mirroring/service/value_util.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/net_utility.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "services/viz/public/cpp/gpu/gpu.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Mock;
using media::cast::FrameSenderConfig;
using media::cast::Packet;
using media::mojom::RemotingStopReason;
using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingSinkMetadata;
using media::mojom::RemotingSinkMetadataPtr;
using mirroring::mojom::SessionType;
using mirroring::mojom::SessionError;

namespace mirroring {

namespace {

class MockRemotingSource final : public media::mojom::RemotingSource {
 public:
  MockRemotingSource() {}
  ~MockRemotingSource() override {}

  void Bind(mojo::PendingReceiver<media::mojom::RemotingSource> receiver) {
    receiver_.Bind(std::move(receiver));
  }

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
};

}  // namespace

class SessionTest : public mojom::ResourceProvider,
                    public mojom::SessionObserver,
                    public mojom::CastMessageChannel,
                    public ::testing::Test {
 public:
  SessionTest() : receiver_endpoint_(media::cast::test::GetFreeLocalPort()) {}

  ~SessionTest() override { task_environment_.RunUntilIdle(); }

 protected:
  // mojom::SessionObserver implemenation.
  MOCK_METHOD1(OnError, void(SessionError));
  MOCK_METHOD0(DidStart, void());
  MOCK_METHOD0(DidStop, void());

  MOCK_METHOD0(OnGetVideoCaptureHost, void());
  MOCK_METHOD0(OnGetNetworkContext, void());
  MOCK_METHOD0(OnCreateAudioStream, void());
  MOCK_METHOD0(OnConnectToRemotingSource, void());

  // Called when sends an outbound message.
  MOCK_METHOD1(OnOutboundMessage, void(const std::string& message_type));

  // mojom::CastMessageHandler implementation. For outbound messages.
  void Send(mojom::CastMessagePtr message) override {
    EXPECT_TRUE(message->message_namespace == mojom::kWebRtcNamespace ||
                message->message_namespace == mojom::kRemotingNamespace);
    std::unique_ptr<base::Value> value =
        base::JSONReader::ReadDeprecated(message->json_format_data);
    ASSERT_TRUE(value);
    std::string message_type;
    EXPECT_TRUE(GetString(*value, "type", &message_type));
    if (message_type == "OFFER") {
      EXPECT_TRUE(GetInt(*value, "seqNum", &offer_sequence_number_));
    } else if (message_type == "GET_CAPABILITIES") {
      EXPECT_TRUE(GetInt(*value, "seqNum", &capability_sequence_number_));
    }
    OnOutboundMessage(message_type);
  }

  // mojom::ResourceProvider implemenation.
  void BindGpu(mojo::PendingReceiver<viz::mojom::Gpu> receiver) override {}
  void GetVideoCaptureHost(
      mojo::PendingReceiver<media::mojom::VideoCaptureHost> receiver) override {
    video_host_ = std::make_unique<FakeVideoCaptureHost>(std::move(receiver));
    OnGetVideoCaptureHost();
  }

  void GetNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver) override {
    network_context_ =
        std::make_unique<MockNetworkContext>(std::move(receiver));
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
    remoting_source_.Bind(std::move(receiver));
    OnConnectToRemotingSource();
  }

  void SendAnswer() {
    ASSERT_TRUE(session_);
    std::vector<FrameSenderConfig> audio_configs;
    std::vector<FrameSenderConfig> video_configs;
    if (session_type_ != SessionType::VIDEO_ONLY) {
      if (cast_mode_ == "remoting") {
        audio_configs.emplace_back(MirrorSettings::GetDefaultAudioConfig(
            media::cast::RtpPayloadType::REMOTE_AUDIO,
            media::cast::Codec::CODEC_AUDIO_REMOTE));
      } else {
        EXPECT_EQ("mirroring", cast_mode_);
        audio_configs.emplace_back(MirrorSettings::GetDefaultAudioConfig(
            media::cast::RtpPayloadType::AUDIO_OPUS,
            media::cast::Codec::CODEC_AUDIO_OPUS));
      }
    }
    if (session_type_ != SessionType::AUDIO_ONLY) {
      if (cast_mode_ == "remoting") {
        video_configs.emplace_back(MirrorSettings::GetDefaultVideoConfig(
            media::cast::RtpPayloadType::REMOTE_VIDEO,
            media::cast::Codec::CODEC_VIDEO_REMOTE));
      } else {
        EXPECT_EQ("mirroring", cast_mode_);
        video_configs.emplace_back(MirrorSettings::GetDefaultVideoConfig(
            media::cast::RtpPayloadType::VIDEO_VP8,
            media::cast::Codec::CODEC_VIDEO_VP8));
      }
    }

    auto answer = std::make_unique<Answer>();
    answer->udp_port = receiver_endpoint_.port();
    answer->cast_mode = cast_mode_;
    answer->supports_get_status = true;
    const int number_of_configs = audio_configs.size() + video_configs.size();
    for (int i = 0; i < number_of_configs; ++i) {
      answer->send_indexes.push_back(i);
      answer->ssrcs.push_back(31 + i);  // Arbitrary receiver SSRCs.
    }

    ReceiverResponse response;
    response.result = "ok";
    response.type = ResponseType::ANSWER;
    response.sequence_number = offer_sequence_number_;
    response.answer = std::move(answer);

    session_->OnAnswer(audio_configs, video_configs, response);
    task_environment_.RunUntilIdle();
  }

  // Create a mirroring session. Expect to send OFFER message.
  void CreateSession(SessionType session_type) {
    session_type_ = session_type;
    mojom::SessionParametersPtr session_params =
        mojom::SessionParameters::New();
    session_params->receiver_address = receiver_endpoint_.address();
    session_params->type = session_type_;
    session_params->receiver_model_name = "Chromecast";
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
    session_ = std::make_unique<Session>(
        std::move(session_params), gfx::Size(1920, 1080),
        std::move(session_observer_remote), std::move(resource_provider_remote),
        std::move(outbound_channel_remote),
        inbound_channel_.BindNewPipeAndPassReceiver(), nullptr);
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
  }

  // Starts the mirroring session.
  void StartSession() {
    ASSERT_TRUE(cast_mode_ == "mirroring");
    // Except mirroing session starts after receiving ANSWER message.
    const int num_to_get_video_host =
        session_type_ == SessionType::AUDIO_ONLY ? 0 : 1;
    const int num_to_create_audio_stream =
        session_type_ == SessionType::VIDEO_ONLY ? 0 : 1;
    EXPECT_CALL(*this, OnGetVideoCaptureHost()).Times(num_to_get_video_host);
    EXPECT_CALL(*this, OnCreateAudioStream()).Times(num_to_create_audio_stream);
    EXPECT_CALL(*this, OnError(_)).Times(0);
    EXPECT_CALL(*this, OnOutboundMessage("GET_STATUS")).Times(AtLeast(1));
    EXPECT_CALL(*this, OnOutboundMessage("GET_CAPABILITIES")).Times(1);
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

  void CaptureOneVideoFrame() {
    ASSERT_TRUE(cast_mode_ == "mirroring");
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
    ReceiverResponse response;
    response.result = "ok";
    response.type = ResponseType::CAPABILITIES_RESPONSE;
    response.sequence_number = capability_sequence_number_;
    response.capabilities = std::make_unique<ReceiverCapability>();
    response.capabilities->media_caps =
        std::vector<std::string>({"video", "audio", "vp8", "opus"});
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
    remoter_->Start();
    run_loop.Run();
    task_environment_.RunUntilIdle();
    cast_mode_ = "remoting";
    Mock::VerifyAndClear(this);
  }

  void RemotingStarted() {
    ASSERT_TRUE(cast_mode_ == "remoting");
    EXPECT_CALL(remoting_source_, OnStarted()).Times(1);
    SendAnswer();
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  void StopRemoting() {
    ASSERT_TRUE(cast_mode_ == "remoting");
    const RemotingStopReason reason = RemotingStopReason::LOCAL_PLAYBACK;
    // Expect to send OFFER message to fallback on mirroring.
    EXPECT_CALL(*this, OnOutboundMessage("OFFER")).Times(1);
    EXPECT_CALL(remoting_source_, OnStopped(reason)).Times(1);
    remoter_->Stop(reason);
    task_environment_.RunUntilIdle();
    cast_mode_ = "mirroring";
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  const net::IPEndPoint receiver_endpoint_;
  mojo::Receiver<mojom::ResourceProvider> resource_provider_receiver_{this};
  mojo::Receiver<mojom::SessionObserver> session_observer_receiver_{this};
  mojo::Receiver<mojom::CastMessageChannel> outbound_channel_receiver_{this};
  mojo::Remote<mojom::CastMessageChannel> inbound_channel_;
  SessionType session_type_ = SessionType::AUDIO_AND_VIDEO;
  mojo::Remote<media::mojom::Remoter> remoter_;
  MockRemotingSource remoting_source_;
  std::string cast_mode_;
  int32_t offer_sequence_number_ = -1;
  int32_t capability_sequence_number_ = -1;

  std::unique_ptr<Session> session_;
  std::unique_ptr<FakeVideoCaptureHost> video_host_;
  std::unique_ptr<MockNetworkContext> network_context_;

  DISALLOW_COPY_AND_ASSIGN(SessionTest);
};

TEST_F(SessionTest, AudioOnlyMirroring) {
  CreateSession(SessionType::AUDIO_ONLY);
  StartSession();
  StopSession();
}

TEST_F(SessionTest, VideoOnlyMirroring) {
  CreateSession(SessionType::VIDEO_ONLY);
  StartSession();
  CaptureOneVideoFrame();
  StopSession();
}

TEST_F(SessionTest, AudioAndVideoMirroring) {
  CreateSession(SessionType::AUDIO_AND_VIDEO);
  StartSession();
  StopSession();
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
  StopRemoting();
  StopSession();
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

}  // namespace mirroring
