// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/media_remoter.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/default_tick_clock.h"
#include "components/mirroring/service/message_dispatcher.h"
#include "components/mirroring/service/mirror_settings.h"
#include "media/cast/cast_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;
using ::testing::_;
using ::testing::Mock;
using media::mojom::RemotingSinkMetadata;
using media::mojom::RemotingStopReason;
using media::cast::RtpPayloadType;
using media::cast::Codec;

namespace mirroring {

namespace {

class MockRemotingSource final : public media::mojom::RemotingSource {
 public:
  MockRemotingSource() = default;
  ~MockRemotingSource() override = default;

  void Bind(mojo::PendingReceiver<media::mojom::RemotingSource> receiver) {
    receiver_.Bind(std::move(receiver));
  }

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
                         public ::testing::Test {
 public:
  MediaRemoterTest()
      : message_dispatcher_(receiver_.BindNewPipeAndPassRemote(),
                            inbound_channel_.BindNewPipeAndPassReceiver(),
                            error_callback_.Get()),
        sink_metadata_(DefaultSinkMetadata()) {}
  ~MediaRemoterTest() override { task_environment_.RunUntilIdle(); }

 protected:
  MOCK_METHOD1(Send, void(mojom::CastMessagePtr));
  MOCK_METHOD0(OnConnectToRemotingSource, void());
  MOCK_METHOD0(RequestRemotingStreaming, void());
  MOCK_METHOD0(RestartMirroringStreaming, void());

  // MediaRemoter::Client implementation.
  void ConnectToRemotingSource(
      mojo::PendingRemote<media::mojom::Remoter> remoter,
      mojo::PendingReceiver<media::mojom::RemotingSource> source_receiver)
      override {
    remoter_.Bind(std::move(remoter));
    remoting_source_.Bind(std::move(source_receiver));
    OnConnectToRemotingSource();
  }

  void CreateRemoter() {
    EXPECT_FALSE(media_remoter_);
    EXPECT_CALL(*this, OnConnectToRemotingSource()).Times(1);
    EXPECT_CALL(remoting_source_, OnSinkAvailable(_)).Times(1);
    media_remoter_ = std::make_unique<MediaRemoter>(this, sink_metadata_,
                                                    &message_dispatcher_);
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  // Requests to start a remoting session.
  void StartRemoting() {
    ASSERT_TRUE(remoter_);
    EXPECT_CALL(*this, RequestRemotingStreaming()).Times(1);
    remoter_->Start();
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
  }

  // Stops the current remoting session.
  void StopRemoting() {
    ASSERT_TRUE(remoter_);
    EXPECT_CALL(remoting_source_, OnStopped(RemotingStopReason::USER_DISABLED))
        .Times(1);
    EXPECT_CALL(remoting_source_, OnSinkGone()).Times(1);
    EXPECT_CALL(*this, RestartMirroringStreaming()).Times(1);
    remoter_->Stop(media::mojom::RemotingStopReason::USER_DISABLED);
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

  // Signals that a remoting streaming session starts successfully.
  void RemotingStreamingStarted() {
    ASSERT_TRUE(media_remoter_);
    scoped_refptr<media::cast::CastEnvironment> cast_environment =
        new media::cast::CastEnvironment(
            base::DefaultTickClock::GetInstance(),
            task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMainThreadTaskRunner());
    EXPECT_CALL(remoting_source_, OnStarted()).Times(1);
    media_remoter_->StartRpcMessaging(
        cast_environment, nullptr, media::cast::FrameSenderConfig(),
        MirrorSettings::GetDefaultVideoConfig(RtpPayloadType::REMOTE_VIDEO,
                                              Codec::CODEC_VIDEO_REMOTE));
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(&remoting_source_);
  }

  // Signals that mirroring is resumed successfully.
  void MirroringResumed() {
    EXPECT_CALL(remoting_source_, OnSinkAvailable(_)).Times(1);
    media_remoter_->OnMirroringResumed();
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(&remoting_source_);
  }

  // Signals that remoting session failed to start.
  void RemotingStartFailed() {
    ASSERT_TRUE(media_remoter_);
    EXPECT_CALL(remoting_source_, OnStartFailed(_)).Times(1);
    EXPECT_CALL(remoting_source_, OnSinkGone()).Times(1);
    EXPECT_CALL(*this, RestartMirroringStreaming()).Times(1);
    media_remoter_->OnRemotingFailed();
    task_environment_.RunUntilIdle();
    Mock::VerifyAndClear(this);
    Mock::VerifyAndClear(&remoting_source_);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Receiver<mojom::CastMessageChannel> receiver_{this};
  base::MockCallback<MessageDispatcher::ErrorCallback> error_callback_;
  mojo::Remote<mojom::CastMessageChannel> inbound_channel_;
  MessageDispatcher message_dispatcher_;
  const media::mojom::RemotingSinkMetadata sink_metadata_;
  MockRemotingSource remoting_source_;
  mojo::Remote<media::mojom::Remoter> remoter_;
  std::unique_ptr<MediaRemoter> media_remoter_;

  DISALLOW_COPY_AND_ASSIGN(MediaRemoterTest);
};

TEST_F(MediaRemoterTest, StartAndStopRemoting) {
  CreateRemoter();
  StartRemoting();
  RemotingStreamingStarted();
  StopRemoting();
}

TEST_F(MediaRemoterTest, StopRemotingWhileStarting) {
  CreateRemoter();
  // Starts a remoting session.
  StartRemoting();
  // Immediately stops the remoting session while not started yet.
  StopRemoting();

  // Signals that successfully switch to mirroring.
  MirroringResumed();
  // Now remoting can be started again.
  StartRemoting();
}

TEST_F(MediaRemoterTest, RemotingStartFailed) {
  CreateRemoter();
  StartRemoting();
  RemotingStartFailed();
}

TEST_F(MediaRemoterTest, SwitchBetweenMultipleSessions) {
  CreateRemoter();

  // Start a remoting session.
  StartRemoting();
  RemotingStreamingStarted();

  // Stop the remoting session and switch to mirroring.
  StopRemoting();
  MirroringResumed();

  // Switch to remoting again.
  StartRemoting();
  RemotingStreamingStarted();

  // Switch to mirroring again.
  StopRemoting();
  MirroringResumed();
}

}  // namespace mirroring
