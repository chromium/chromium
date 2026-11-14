// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/media_remoter.h"

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/default_tick_clock.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "components/mirroring/service/mirror_settings.h"
#include "components/mirroring/service/remoting_sender.h"
#include "components/mirroring/service/rpc_dispatcher.h"
#include "components/openscreen_platform/task_runner.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"
#include "media/cast/test/openscreen_test_helpers.h"
#include "media/cast/test/test_with_cast_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::mojom::RemotingSinkMetadata;
using media::mojom::RemotingStopReason;
using mirroring::mojom::SessionType;
using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;

namespace mirroring {

namespace {

// Mojo handles used for managing the remoting data streams.
struct DataStreamHandles {
  mojo::ScopedDataPipeProducerHandle audio_producer_handle;
  mojo::ScopedDataPipeProducerHandle video_producer_handle;

  mojo::PendingRemote<media::mojom::RemotingDataStreamSender>
      audio_stream_sender;
  mojo::PendingRemote<media::mojom::RemotingDataStreamSender>
      video_stream_sender;
};

class MockRemotingSource : public media::mojom::RemotingSource {
 public:
  MockRemotingSource() = default;
  ~MockRemotingSource() override = default;

  void Bind(mojo::PendingReceiver<media::mojom::RemotingSource> receiver) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD0(OnSinkGone, void());
  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD1(OnStartFailed, void(media::mojom::RemotingStartFailReason));
  MOCK_METHOD1(OnMessageFromSink, void(const std::vector<uint8_t>&));
  MOCK_METHOD1(OnStopped, void(RemotingStopReason));
  MOCK_METHOD1(OnSinkAvailable, void(const RemotingSinkMetadata&));
  void OnSinkAvailable(
      media::mojom::RemotingSinkMetadataPtr metadata) override {
    OnSinkAvailable(*metadata);
  }

 private:
  mojo::Receiver<media::mojom::RemotingSource> receiver_{this};
};

class MockRpcDispatcher : public RpcDispatcher {
 public:
  MockRpcDispatcher() = default;
  ~MockRpcDispatcher() override = default;

  MOCK_METHOD2(Subscribe, void(ResponseCallback, ErrorCallback));
  MOCK_METHOD0(Unsubscribe, void());
  MOCK_METHOD1(SendOutboundMessage, bool(base::span<const uint8_t> message));
};

RemotingSinkMetadata DefaultSinkMetadata() {
  RemotingSinkMetadata metadata;
  metadata.features.push_back(media::mojom::RemotingSinkFeature::RENDERING);
  metadata.video_capabilities.push_back(
      media::mojom::RemotingSinkVideoCapability::CODEC_VP8);
  metadata.audio_capabilities.push_back(
      media::mojom::RemotingSinkAudioCapability::CODEC_BASELINE_SET);
  metadata.friendly_name = "Test";
  return metadata;
}

}  // namespace

class MediaRemoterTest : public mojom::CastMessageChannel,
                         public MediaRemoter::Client,
                         public media::cast::TestWithCastEnvironment {
 public:
  MediaRemoterTest() : sink_metadata_(DefaultSinkMetadata()) {}

  MediaRemoterTest(MediaRemoterTest&&) = delete;
  MediaRemoterTest(const MediaRemoterTest&) = delete;
  MediaRemoterTest& operator=(const MediaRemoterTest&) = delete;
  MediaRemoterTest& operator=(MediaRemoterTest&&) = delete;
  ~MediaRemoterTest() override = default;

 protected:
  // mojom::CastMessageChannel mock implementation (inbound messages).
  MOCK_METHOD1(OnMessage, void(mojom::CastMessagePtr));
  MOCK_METHOD0(OnConnectToRemotingSource, void());
  MOCK_METHOD(void, StartRemoting, (), (override));
  MOCK_METHOD(void, StopRemoting, (RemotingStopReason), (override));

  // MediaRemoter::Client implementation.
  void ConnectToRemotingSource(
      mojo::PendingRemote<media::mojom::Remoter> remoter,
      mojo::PendingReceiver<media::mojom::RemotingSource> source_receiver)
      override {
    remoter_.reset();
    remoter_.Bind(std::move(remoter));
    remoting_source_.Bind(std::move(source_receiver));
    OnConnectToRemotingSource();
  }

  std::unique_ptr<media::mojom::RemotingDataStreamSender>
  CreateRemotingDataStreamSender(
      bool is_audio,
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> receiver,
      base::OnceClosure error_callback) override {
    if (!openscreen_test_senders_) {
      return nullptr;
    }
    auto& sender = is_audio ? openscreen_test_senders_->audio_sender
                            : openscreen_test_senders_->video_sender;
    if (!sender) {
      return nullptr;
    }

    MirrorSettings mirror_settings(/*target_playout_delay=*/std::nullopt);
    auto config =
        is_audio ? mirror_settings.GetAudioConfig(media::AudioCodec::kUnknown)
                 : mirror_settings.GetVideoConfig(media::VideoCodec::kUnknown);
    return std::make_unique<RemotingSender>(
        cast_environment(), std::move(sender), config, std::move(pipe),
        std::move(receiver), std::move(error_callback));
  }

  void CreateRemoter() {
    EXPECT_FALSE(media_remoter_);
    EXPECT_CALL(*this, OnConnectToRemotingSource());
    EXPECT_CALL(remoting_source_, OnSinkAvailable(_));
    media_remoter_ =
        std::make_unique<MediaRemoter>(*this, sink_metadata_, rpc_dispatcher_);
    RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  // The source asks to start a remoting session.
  void SourceStartsRemoting() {
    ASSERT_TRUE(remoter_);
    EXPECT_CALL(*this, StartRemoting());
    remoter_->Start();
    RunUntilIdle();
    Mock::VerifyAndClear(this);
  }

  // The source stops the current remoting session.
  void SourceStopsRemoting() {
    ASSERT_TRUE(remoter_);
    EXPECT_CALL(remoting_source_, OnStopped(RemotingStopReason::USER_DISABLED));
    EXPECT_CALL(remoting_source_, OnSinkGone());
    EXPECT_CALL(rpc_dispatcher_, Unsubscribe());
    EXPECT_CALL(*this, StopRemoting(RemotingStopReason::USER_DISABLED));
    remoter_->Stop(media::mojom::RemotingStopReason::USER_DISABLED);
    RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  // Signals that a remoting streaming session starts successfully.
  void RemotingStreamingStarted() {
    ASSERT_TRUE(media_remoter_);

    // Cannot have two instances of test senders at once.
    openscreen_test_senders_.reset();
    openscreen_test_senders_ =
        std::make_unique<media::cast::OpenscreenTestSenders>(
            media::cast::OpenscreenTestSenders::Config(
                base::SequencedTaskRunner::GetCurrentDefault(),
                GetMockTickClock(),
                openscreen::cast::RtpPayloadType::kAudioVarious,
                openscreen::cast::RtpPayloadType::kVideoVarious));
    media_remoter_->OnRemotingStarted();
    RunUntilIdle();
    Mock::VerifyAndClear(&remoting_source_);
  }

  // Signals that the client is ready to accept remoting requests again.
  void RemoterReady(bool is_remoting_disabled) {
    // When MediaRemoter is in the REMOTING_DISABLED state, it should not notify
    // its remoting_source_ about available sinks.
    EXPECT_CALL(remoting_source_, OnSinkAvailable(_))
        .Times(is_remoting_disabled ? 0 : 1);
    media_remoter_->OnReadyForRemoting();
    RunUntilIdle();
    Mock::VerifyAndClear(&remoting_source_);
  }

  // Signals that remoting session failed to start. The remoter must not
  // call back into the client.
  void RemotingStartFailed() {
    ASSERT_TRUE(media_remoter_);
    EXPECT_CALL(remoting_source_, OnStartFailed(_));
    EXPECT_CALL(remoting_source_, OnSinkGone());
    EXPECT_CALL(*this, StopRemoting(_)).Times(0);
    media_remoter_->OnRemotingStartFailed();
    RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  void StartDataStreams(SessionType session_type) {
    static constexpr int kDataPipeCapacity = 1000;
    data_stream_handles_ = std::make_unique<DataStreamHandles>();
    mojo::ScopedDataPipeConsumerHandle audio_consumer_handle;
    mojo::ScopedDataPipeConsumerHandle video_consumer_handle;

    if (session_type != SessionType::VIDEO_ONLY) {
      EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(
                                    kDataPipeCapacity,
                                    data_stream_handles_->audio_producer_handle,
                                    audio_consumer_handle));
    }

    if (session_type != SessionType::AUDIO_ONLY) {
      EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(
                                    kDataPipeCapacity,
                                    data_stream_handles_->video_producer_handle,
                                    video_consumer_handle));
    }

    remoter_->StartDataStreams(std::move(audio_consumer_handle),
                               std::move(video_consumer_handle),
                               (session_type != SessionType::VIDEO_ONLY)
                                   ? data_stream_handles_->audio_stream_sender
                                         .InitWithNewPipeAndPassReceiver()
                                   : mojo::NullReceiver(),
                               (session_type != SessionType::AUDIO_ONLY)
                                   ? data_stream_handles_->video_stream_sender
                                         .InitWithNewPipeAndPassReceiver()
                                   : mojo::NullReceiver());
  }

  testing::StrictMock<MockRemotingSource>& remoting_source() {
    return remoting_source_;
  }

  testing::StrictMock<MockRpcDispatcher>& rpc_dispatcher() {
    return rpc_dispatcher_;
  }

  mojo::Remote<media::mojom::Remoter>& remoter() { return remoter_; }

  // Signals that the client stopped remoting. The remoter must not call back
  // into the client.
  void ClientStopsRemoting(RemotingStopReason reason) {
    ASSERT_TRUE(media_remoter_);
    EXPECT_CALL(remoting_source_, OnStopped(reason));
    EXPECT_CALL(remoting_source_, OnSinkGone());
    EXPECT_CALL(rpc_dispatcher_, Unsubscribe());
    EXPECT_CALL(*this, StopRemoting(_)).Times(0);
    media_remoter_->OnRemotingStopped(reason);
    RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
    Mock::VerifyAndClear(&rpc_dispatcher_);
  }

  // Signals that the remoting source changed.
  void SourceChanged(bool expect_sink_available) {
    ASSERT_TRUE(media_remoter_);
    EXPECT_CALL(*this, OnConnectToRemotingSource());
    EXPECT_CALL(remoting_source_, OnSinkAvailable(_))
        .Times(expect_sink_available ? 1 : 0);
    media_remoter_->OnSourceChanged();
    RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  void DestroyRemoter() { media_remoter_.reset(); }

  void DisconnectAudioStream() {
    data_stream_handles_->audio_stream_sender.reset();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  mojo::Receiver<mojom::CastMessageChannel> receiver_{this};
  mojo::Remote<mojom::CastMessageChannel> inbound_channel_;
  testing::StrictMock<MockRpcDispatcher> rpc_dispatcher_;
  const media::mojom::RemotingSinkMetadata sink_metadata_;
  testing::StrictMock<MockRemotingSource> remoting_source_;
  mojo::Remote<media::mojom::Remoter> remoter_;

  // Configured for use by the media remoter.
  std::unique_ptr<media::cast::OpenscreenTestSenders> openscreen_test_senders_;
  std::unique_ptr<DataStreamHandles> data_stream_handles_;
  std::unique_ptr<MediaRemoter> media_remoter_;
};

TEST_F(MediaRemoterTest, StartAndStopRemoting) {
  CreateRemoter();
  SourceStartsRemoting();
  EXPECT_CALL(remoting_source(), OnStarted());
  EXPECT_CALL(rpc_dispatcher(), Subscribe(_, _));
  RemotingStreamingStarted();
  StartDataStreams(SessionType::AUDIO_AND_VIDEO);
  SourceStopsRemoting();
}

TEST_F(MediaRemoterTest, StartAndStopRemotingAudioOnly) {
  CreateRemoter();
  SourceStartsRemoting();
  EXPECT_CALL(remoting_source(), OnStarted());
  EXPECT_CALL(rpc_dispatcher(), Subscribe(_, _));
  RemotingStreamingStarted();
  StartDataStreams(SessionType::AUDIO_ONLY);
  SourceStopsRemoting();
}

TEST_F(MediaRemoterTest, StartAndStopRemotingVideoOnly) {
  CreateRemoter();
  SourceStartsRemoting();
  EXPECT_CALL(remoting_source(), OnStarted());
  EXPECT_CALL(rpc_dispatcher(), Subscribe(_, _));
  RemotingStreamingStarted();
  StartDataStreams(SessionType::VIDEO_ONLY);
  SourceStopsRemoting();
}

TEST_F(MediaRemoterTest, BrowserDataSendFailureAllowsRemotingAgain) {
  CreateRemoter();
  SourceStartsRemoting();
  EXPECT_CALL(remoting_source(), OnStarted());
  EXPECT_CALL(rpc_dispatcher(), Subscribe(_, _));
  RemotingStreamingStarted();

  EXPECT_CALL(remoting_source(),
              OnStopped(RemotingStopReason::DATA_SEND_FAILED));
  EXPECT_CALL(remoting_source(), OnSinkGone());
  EXPECT_CALL(rpc_dispatcher(), Unsubscribe());
  EXPECT_CALL(*this, StopRemoting(RemotingStopReason::DATA_SEND_FAILED));
  remoter()->Stop(RemotingStopReason::DATA_SEND_FAILED);
  remoter().FlushForTesting();
  remoting_source().FlushForTesting();

  RemoterReady(/*is_remoting_disabled=*/false);
  SourceStartsRemoting();
  SourceStopsRemoting();
}

TEST_F(MediaRemoterTest, StartRemotingWithoutCallingStart) {
  CreateRemoter();
  // Should fail since the source did not ask to start remoting.
  EXPECT_CALL(remoting_source(), OnStarted()).Times(0);
  RemotingStreamingStarted();
}

TEST_F(MediaRemoterTest, StopRemotingWhileStarting) {
  CreateRemoter();
  // Starts a remoting session.
  SourceStartsRemoting();
  // Immediately stops the remoting session while not started yet.
  SourceStopsRemoting();

  // Signals that the client is ready to accept remoting requests again.
  RemoterReady(/* is_remoting_disabled */ false);
  // Now remoting can be started again.
  SourceStartsRemoting();

  // Destroying the remoter ends the pending start without calling the
  // client.
  EXPECT_CALL(rpc_dispatcher(), Unsubscribe());
  EXPECT_CALL(*this, StopRemoting(_)).Times(0);
  DestroyRemoter();
}

TEST_F(MediaRemoterTest, RemotingStartFailed) {
  CreateRemoter();
  SourceStartsRemoting();
  RemotingStartFailed();
  RemoterReady(/* is_remoting_disabled */ true);
}

TEST_F(MediaRemoterTest, ClientStopsRemoting) {
  CreateRemoter();
  SourceStartsRemoting();
  EXPECT_CALL(remoting_source(), OnStarted());
  EXPECT_CALL(rpc_dispatcher(), Subscribe(_, _));
  RemotingStreamingStarted();
  StartDataStreams(SessionType::AUDIO_AND_VIDEO);

  ClientStopsRemoting(RemotingStopReason::LOCAL_PLAYBACK);

  // Remoting can start again when the client is ready.
  RemoterReady(/* is_remoting_disabled */ false);
  SourceStartsRemoting();
  SourceStopsRemoting();
}

TEST_F(MediaRemoterTest, ClientStopsRemotingWhileStarting) {
  CreateRemoter();
  SourceStartsRemoting();
  ClientStopsRemoting(RemotingStopReason::LOCAL_PLAYBACK);

  // A late answer to the cancelled start is ignored.
  EXPECT_CALL(remoting_source(), OnStarted()).Times(0);
  RemotingStreamingStarted();
}

TEST_F(MediaRemoterTest, SourceChangedWhileReady) {
  CreateRemoter();
  SourceChanged(/*expect_sink_available=*/true);
  SourceStartsRemoting();
  SourceStopsRemoting();
}

TEST_F(MediaRemoterTest, SourceChangedAfterClientStopsRemoting) {
  CreateRemoter();
  SourceStartsRemoting();
  EXPECT_CALL(remoting_source(), OnStarted());
  EXPECT_CALL(rpc_dispatcher(), Subscribe(_, _));
  RemotingStreamingStarted();
  ClientStopsRemoting(RemotingStopReason::LOCAL_PLAYBACK);

  // The new source only hears about the sink when the client is ready.
  SourceChanged(/*expect_sink_available=*/false);
  RemoterReady(/* is_remoting_disabled */ false);
  SourceStartsRemoting();
  SourceStopsRemoting();
}

TEST_F(MediaRemoterTest, SwitchBetweenMultipleSessions) {
  CreateRemoter();

  // Start a remoting session.
  SourceStartsRemoting();
  EXPECT_CALL(remoting_source(), OnStarted());
  EXPECT_CALL(rpc_dispatcher(), Subscribe(_, _));
  RemotingStreamingStarted();
  StartDataStreams(SessionType::AUDIO_AND_VIDEO);

  // Stop the remoting session.
  SourceStopsRemoting();
  RemoterReady(/* is_remoting_disabled */ false);

  // Switch to remoting again.
  SourceStartsRemoting();
  EXPECT_CALL(remoting_source(), OnStarted());
  EXPECT_CALL(rpc_dispatcher(), Subscribe(_, _));
  RemotingStreamingStarted();
  StartDataStreams(SessionType::AUDIO_AND_VIDEO);

  // Stop the remoting session again.
  SourceStopsRemoting();
  RemoterReady(/* is_remoting_disabled */ false);
}

TEST_F(MediaRemoterTest, ReceivesRpcMessagesAndSendsOutboundMessages) {
  CreateRemoter();
  SourceStartsRemoting();
  EXPECT_CALL(remoting_source(), OnStarted());
  RpcDispatcher::ResponseCallback response_callback;
  EXPECT_CALL(rpc_dispatcher(), Subscribe(_, _))
      .WillOnce(
          [&response_callback](RpcDispatcher::ResponseCallback cb,
                               RpcDispatcher::ErrorCallback /*error_cb*/) {
            response_callback = std::move(cb);
          });
  RemotingStreamingStarted();

  ASSERT_TRUE(response_callback);
  const std::vector<uint8_t> kInboundMessage{1, 2, 3, 4};
  EXPECT_CALL(remoting_source(), OnMessageFromSink(kInboundMessage));
  response_callback.Run(kInboundMessage);
  remoting_source().FlushForTesting();

  const std::vector<uint8_t> kOutboundMessage{5, 6, 7, 8};
  EXPECT_CALL(rpc_dispatcher(),
              SendOutboundMessage(testing::ElementsAre(5, 6, 7, 8)))
      .WillOnce(testing::Return(true));
  remoter()->SendMessageToSink(kOutboundMessage);
  remoter().FlushForTesting();

  SourceStopsRemoting();
}

TEST_F(MediaRemoterTest, RpcErrorStopsRemoting) {
  CreateRemoter();
  SourceStartsRemoting();
  EXPECT_CALL(remoting_source(), OnStarted());
  RpcDispatcher::ErrorCallback error_callback;
  EXPECT_CALL(rpc_dispatcher(), Subscribe(_, _))
      .WillOnce([&error_callback](RpcDispatcher::ResponseCallback /*cb*/,
                                  RpcDispatcher::ErrorCallback error_cb) {
        error_callback = std::move(error_cb);
      });
  RemotingStreamingStarted();
  StartDataStreams(SessionType::AUDIO_AND_VIDEO);

  ASSERT_TRUE(error_callback);
  EXPECT_CALL(remoting_source(),
              OnStopped(RemotingStopReason::MESSAGE_SEND_FAILED));
  EXPECT_CALL(remoting_source(), OnSinkGone());
  EXPECT_CALL(rpc_dispatcher(), Unsubscribe());
  EXPECT_CALL(*this, StopRemoting(RemotingStopReason::MESSAGE_SEND_FAILED));
  error_callback.Run();
  remoting_source().FlushForTesting();

  // Remoting should remain disabled after an RPC error.
  RemoterReady(/* is_remoting_disabled */ true);
}

TEST_F(MediaRemoterTest, RpcErrorCanDestroyRemoter) {
  CreateRemoter();
  SourceStartsRemoting();
  EXPECT_CALL(remoting_source(), OnStarted());
  RpcDispatcher::ErrorCallback error_callback;
  EXPECT_CALL(rpc_dispatcher(), Subscribe(_, _))
      .WillOnce([&error_callback](RpcDispatcher::ResponseCallback,
                                  RpcDispatcher::ErrorCallback error_cb) {
        error_callback = std::move(error_cb);
      });
  RemotingStreamingStarted();

  ASSERT_TRUE(error_callback);
  EXPECT_CALL(remoting_source(),
              OnStopped(RemotingStopReason::MESSAGE_SEND_FAILED));
  EXPECT_CALL(remoting_source(), OnSinkGone());
  EXPECT_CALL(rpc_dispatcher(), Unsubscribe());
  EXPECT_CALL(*this, StopRemoting(RemotingStopReason::MESSAGE_SEND_FAILED))
      .WillOnce([this] { DestroyRemoter(); });
  error_callback.Run();
  remoting_source().FlushForTesting();
}

TEST_F(MediaRemoterTest, DataStreamErrorDisablesRemoting) {
  CreateRemoter();
  SourceStartsRemoting();
  EXPECT_CALL(remoting_source(), OnStarted());
  EXPECT_CALL(rpc_dispatcher(), Subscribe(_, _));
  RemotingStreamingStarted();
  StartDataStreams(SessionType::AUDIO_ONLY);
  remoter().FlushForTesting();

  base::RunLoop run_loop;
  EXPECT_CALL(remoting_source(),
              OnStopped(RemotingStopReason::DATA_SEND_FAILED));
  EXPECT_CALL(remoting_source(), OnSinkGone());
  EXPECT_CALL(rpc_dispatcher(), Unsubscribe());
  EXPECT_CALL(*this, StopRemoting(RemotingStopReason::DATA_SEND_FAILED))
      .WillOnce([&run_loop] { run_loop.Quit(); });
  DisconnectAudioStream();
  run_loop.Run();
  remoting_source().FlushForTesting();

  RemoterReady(/*is_remoting_disabled=*/true);
}

}  // namespace mirroring
