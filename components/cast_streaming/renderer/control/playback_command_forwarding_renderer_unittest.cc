// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/control/playback_command_forwarding_renderer.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/cast_streaming/common/public/mojom/renderer_controller.mojom.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_streaming {
namespace {

class MockMojoRendererClient : public media::mojom::RendererClient {
 public:
  explicit MockMojoRendererClient(
      mojo::PendingAssociatedReceiver<media::mojom::RendererClient> receiver)
      : receiver_(this, std::move(receiver)) {}

  MockMojoRendererClient(const MockMojoRendererClient&) = delete;
  MockMojoRendererClient& operator=(const MockMojoRendererClient&) = delete;

  ~MockMojoRendererClient() override = default;

  MOCK_METHOD0(FlushCallback, void());
  MOCK_METHOD1(InitializeCallback, void(bool));

  // mojom::RendererClient implementation.
  MOCK_METHOD3(OnTimeUpdate,
               void(base::TimeDelta time,
                    base::TimeDelta max_time,
                    base::TimeTicks capture_time));
  MOCK_METHOD2(OnBufferingStateChange,
               void(media::BufferingState state,
                    media::BufferingStateChangeReason reason));
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD1(OnError, void(const media::PipelineStatus& status));
  MOCK_METHOD1(OnVideoOpacityChange, void(bool opaque));
  MOCK_METHOD1(OnAudioConfigChange, void(const media::AudioDecoderConfig&));
  MOCK_METHOD1(OnVideoConfigChange, void(const media::VideoDecoderConfig&));
  MOCK_METHOD1(OnVideoNaturalSizeChange, void(const gfx::Size& size));
  MOCK_METHOD1(OnStatisticsUpdate,
               void(const media::PipelineStatistics& stats));
  MOCK_METHOD1(OnWaiting, void(media::WaitingReason));
  MOCK_METHOD1(OnDurationChange, void(base::TimeDelta duration));
  MOCK_METHOD1(OnRemotePlayStateChange, void(media::MediaStatus::State state));

 private:
  mojo::AssociatedReceiver<media::mojom::RendererClient> receiver_;
};

}  // namespace

class PlaybackCommandForwardingRendererTest : public testing::Test {
 public:
  PlaybackCommandForwardingRendererTest() {
    auto mock_renderer =
        std::make_unique<testing::StrictMock<media::MockRenderer>>();
    mock_renderer_ = mock_renderer.get();
    renderer_ = std::make_unique<PlaybackCommandForwardingRenderer>(
        std::move(mock_renderer), task_environment_.GetMainThreadTaskRunner(),
        remote_.BindNewPipeAndPassReceiver());

    mojo_renderer_client_ =
        std::make_unique<testing::StrictMock<MockMojoRendererClient>>(
            remote_client_.InitWithNewEndpointAndPassReceiver());
  }

  ~PlaybackCommandForwardingRendererTest() override {
    mock_renderer_ = nullptr;
  }

 protected:
  void CallInitialize() {
    auto init_cb = base::BindOnce(
        &PlaybackCommandForwardingRendererTest::OnInitializationComplete,
        base::Unretained(this));

    EXPECT_CALL(*mock_renderer_, OnInitialize(&mock_media_resource_,
                                              renderer_.get(), testing::_))
        .WillOnce([this](media::MediaResource* media_resource,
                         media::RendererClient* client,
                         media::PipelineStatusCallback& init_cb) {
          task_environment_.GetMainThreadTaskRunner()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(init_cb), media::PIPELINE_OK));
        });

    renderer_->Initialize(&mock_media_resource_, &mock_renderer_client_,
                          std::move(init_cb));
  }

  media::RendererClient* renderer_client() { return renderer_.get(); }

  MOCK_METHOD1(OnInitializationComplete, void(media::PipelineStatus));

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  mojo::Remote<media::mojom::Renderer> remote_;
  mojo::PendingAssociatedRemote<media::mojom::RendererClient> remote_client_;

  raw_ptr<media::MockRenderer, DanglingUntriaged> mock_renderer_;
  testing::StrictMock<media::MockMediaResource> mock_media_resource_;
  testing::StrictMock<media::MockRendererClient> mock_renderer_client_;
  std::unique_ptr<PlaybackCommandForwardingRenderer> renderer_;
  std::unique_ptr<testing::StrictMock<MockMojoRendererClient>>
      mojo_renderer_client_;
};

TEST_F(PlaybackCommandForwardingRendererTest, StartPlaybackAsMirroring) {
  remote_->SetPlaybackRate(1.0);
  remote_->StartPlayingFrom(base::TimeDelta{});
  task_environment_.RunUntilIdle();

  CallInitialize();

  EXPECT_CALL(*this, OnInitializationComplete(
                         media::HasStatusCode(media::PIPELINE_OK)));
  EXPECT_CALL(*mock_renderer_, SetPlaybackRate(1.0));
  EXPECT_CALL(*mock_renderer_, StartPlayingFrom(testing::_));
  task_environment_.RunUntilIdle();
}

TEST_F(PlaybackCommandForwardingRendererTest, MojoCallsCorrectlyForwarded) {
  CallInitialize();
  EXPECT_CALL(*this, OnInitializationComplete(
                         media::HasStatusCode(media::PIPELINE_OK)));
  task_environment_.RunUntilIdle();

  EXPECT_CALL(*mock_renderer_, StartPlayingFrom(base::Seconds(42)));
  remote_->StartPlayingFrom(base::Seconds(42));
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_renderer_);

  EXPECT_CALL(*mock_renderer_, SetPlaybackRate(12.34));
  remote_->SetPlaybackRate(12.34);
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_renderer_);

  EXPECT_CALL(*mock_renderer_, OnFlush(testing::_))
      .WillOnce([](base::OnceClosure& callback) { std::move(callback).Run(); });
  EXPECT_CALL(*mojo_renderer_client_, FlushCallback());
  remote_->Flush(base::BindOnce(&MockMojoRendererClient::FlushCallback,
                                base::Unretained(mojo_renderer_client_.get())));
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_renderer_);

  EXPECT_CALL(*mock_renderer_, SetVolume(43.21));
  remote_->SetVolume(43.21);
  task_environment_.RunUntilIdle();
}

TEST_F(PlaybackCommandForwardingRendererTest, RendererClientCallbacksCalled) {
  CallInitialize();
  EXPECT_CALL(*this, OnInitializationComplete(
                         media::HasStatusCode(media::PIPELINE_OK)));
  task_environment_.RunUntilIdle();

  EXPECT_CALL(*mojo_renderer_client_, InitializeCallback(true));
  remote_->Initialize(
      std::move(remote_client_), std::nullopt, nullptr,
      base::BindOnce(&MockMojoRendererClient::InitializeCallback,
                     base::Unretained(mojo_renderer_client_.get())));
  task_environment_.RunUntilIdle();

  const auto status = media::PIPELINE_ERROR_NETWORK;
  EXPECT_CALL(*mojo_renderer_client_, OnError(testing::_));
  EXPECT_CALL(mock_renderer_client_, OnError(media::HasStatusCode(status)));
  renderer_client()->OnError(status);
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mojo_renderer_client_.get());
  testing::Mock::VerifyAndClearExpectations(&mock_renderer_client_);

  EXPECT_CALL(*mojo_renderer_client_, OnEnded());
  EXPECT_CALL(mock_renderer_client_, OnEnded());
  renderer_client()->OnEnded();
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mojo_renderer_client_.get());
  testing::Mock::VerifyAndClearExpectations(&mock_renderer_client_);

  media::PipelineStatistics stats;
  stats.audio_bytes_decoded = 7;
  stats.video_memory_usage = 42;
  EXPECT_CALL(*mojo_renderer_client_, OnStatisticsUpdate(stats));
  EXPECT_CALL(mock_renderer_client_, OnStatisticsUpdate(stats));
  renderer_client()->OnStatisticsUpdate(stats);
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mojo_renderer_client_.get());
  testing::Mock::VerifyAndClearExpectations(&mock_renderer_client_);

  const media::BufferingState state = media::BUFFERING_HAVE_ENOUGH;
  const media::BufferingStateChangeReason reason = media::DEMUXER_UNDERFLOW;
  EXPECT_CALL(*mojo_renderer_client_, OnBufferingStateChange(state, reason));
  EXPECT_CALL(mock_renderer_client_, OnBufferingStateChange(state, reason));
  renderer_client()->OnBufferingStateChange(state, reason);
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mojo_renderer_client_.get());
  testing::Mock::VerifyAndClearExpectations(&mock_renderer_client_);

  const auto wait_reason = media::WaitingReason::kDecoderStateLost;
  EXPECT_CALL(*mojo_renderer_client_, OnWaiting(wait_reason));
  EXPECT_CALL(mock_renderer_client_, OnWaiting(wait_reason));
  renderer_client()->OnWaiting(wait_reason);
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mojo_renderer_client_.get());
  testing::Mock::VerifyAndClearExpectations(&mock_renderer_client_);

  const media::AudioDecoderConfig audio_config(
      media::AudioCodec::kAAC, media::SampleFormat::kSampleFormatU8,
      media::CHANNEL_LAYOUT_STEREO, 3, {},
      media::EncryptionScheme::kUnencrypted);
  EXPECT_CALL(*mojo_renderer_client_, OnAudioConfigChange(testing::_));
  EXPECT_CALL(mock_renderer_client_, OnAudioConfigChange(testing::_));
  renderer_client()->OnAudioConfigChange(audio_config);
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mojo_renderer_client_.get());
  testing::Mock::VerifyAndClearExpectations(&mock_renderer_client_);

  const media::VideoDecoderConfig video_config(
      media::VideoCodec::kH264, media::H264PROFILE_MAIN,
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::VideoTransformation(), gfx::Size{1920, 1080},
      gfx::Rect{1920, 1080}, gfx::Size{1920, 1080}, media::EmptyExtraData(),
      media::EncryptionScheme::kUnencrypted);
  EXPECT_CALL(*mojo_renderer_client_, OnVideoConfigChange(testing::_));
  EXPECT_CALL(mock_renderer_client_, OnVideoConfigChange(testing::_));
  renderer_client()->OnVideoConfigChange(video_config);
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mojo_renderer_client_.get());
  testing::Mock::VerifyAndClearExpectations(&mock_renderer_client_);

  const gfx::Size size{123, 456};
  EXPECT_CALL(*mojo_renderer_client_, OnVideoNaturalSizeChange(size));
  EXPECT_CALL(mock_renderer_client_, OnVideoNaturalSizeChange(size));
  renderer_client()->OnVideoNaturalSizeChange(size);
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mojo_renderer_client_.get());
  testing::Mock::VerifyAndClearExpectations(&mock_renderer_client_);

  const bool opacity = true;
  EXPECT_CALL(*mojo_renderer_client_, OnVideoOpacityChange(opacity));
  EXPECT_CALL(mock_renderer_client_, OnVideoOpacityChange(opacity));
  renderer_client()->OnVideoOpacityChange(opacity);
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mojo_renderer_client_.get());
  testing::Mock::VerifyAndClearExpectations(&mock_renderer_client_);

  const std::optional<int> frame_rate = 123;
  EXPECT_CALL(mock_renderer_client_, OnVideoFrameRateChange(frame_rate));
  renderer_client()->OnVideoFrameRateChange(frame_rate);
  task_environment_.RunUntilIdle();
}

}  // namespace cast_streaming
