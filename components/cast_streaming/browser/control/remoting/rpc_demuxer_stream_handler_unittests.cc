// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/control/remoting/rpc_demuxer_stream_handler.h"

#include <memory>
#include <utility>

#include "base/test/task_environment.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/channel_layout.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_util.h"
#include "media/base/sample_format.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace cast_streaming::remoting {
namespace {

ACTION_P(CheckInitializeDemuxerStream, remote_handle, local_handle) {
  const openscreen::cast::RpcMessenger::Handle handle = arg0;
  const std::unique_ptr<openscreen::cast::RpcMessage>& rpc = arg1;

  ASSERT_TRUE(rpc);
  EXPECT_EQ(rpc->proc(), openscreen::cast::RpcMessage::RPC_DS_INITIALIZE);
  EXPECT_EQ(handle, remote_handle);
  ASSERT_TRUE(rpc->has_integer_value());
  EXPECT_EQ(local_handle, rpc->integer_value());
}

ACTION_P(CheckReadUntilCall, remote_handle, local_handle, last_total) {
  const openscreen::cast::RpcMessenger::Handle handle = arg0;
  const std::unique_ptr<openscreen::cast::RpcMessage>& rpc = arg1;

  ASSERT_TRUE(rpc);
  EXPECT_EQ(rpc->proc(), openscreen::cast::RpcMessage::RPC_DS_READUNTIL);
  EXPECT_EQ(handle, remote_handle);
  ASSERT_TRUE(rpc->has_demuxerstream_readuntil_rpc());
  const auto& read_until = rpc->demuxerstream_readuntil_rpc();
  EXPECT_GT(read_until.count(), static_cast<uint32_t>(last_total));
  EXPECT_EQ(read_until.callback_handle(), local_handle);
}

ACTION_P(CheckOnErrorCall, remote_handle) {
  const openscreen::cast::RpcMessenger::Handle handle = arg0;
  const std::unique_ptr<openscreen::cast::RpcMessage>& rpc = arg1;

  ASSERT_TRUE(rpc);
  EXPECT_EQ(rpc->proc(), openscreen::cast::RpcMessage::RPC_DS_ONERROR);
  EXPECT_EQ(handle, remote_handle);
}

ACTION_P(CheckEnableBistreamConverterCall, remote_handle) {
  const openscreen::cast::RpcMessenger::Handle handle = arg0;
  const std::unique_ptr<openscreen::cast::RpcMessage>& rpc = arg1;

  ASSERT_TRUE(rpc);
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_DS_ENABLEBITSTREAMCONVERTER);
  EXPECT_EQ(handle, remote_handle);
}

}  // namespace

class RpcDemuxerStreamHandlerTest : public testing::Test {
 public:
  RpcDemuxerStreamHandlerTest()
      : test_audio_config_(media::AudioCodec::kAAC,
                           media::SampleFormat::kSampleFormatF32,
                           media::CHANNEL_LAYOUT_MONO,
                           10000,
                           media::EmptyExtraData(),
                           media::EncryptionScheme::kUnencrypted),
        test_video_config_(media::VideoCodec::kH264,
                           media::VideoCodecProfile::H264PROFILE_MAIN,
                           media::VideoDecoderConfig::AlphaMode::kIsOpaque,
                           media::VideoColorSpace::JPEG(),
                           media::VideoTransformation(),
                           {1920, 1080},
                           {1920, 1080},
                           {1920, 1080},
                           media::EmptyExtraData(),
                           media::EncryptionScheme::kUnencrypted),
        stream_handler_(
            task_environment_.GetMainThreadTaskRunner(),
            &client_,
            base::BindRepeating(&RpcDemuxerStreamHandlerTest::GetHandle,
                                base::Unretained(this)),
            base::BindRepeating(&RpcDemuxerStreamHandlerTest::SendMessage,
                                base::Unretained(this))) {
    EXPECT_CALL(*this, GetHandle())
        .WillOnce(Return(audio_local_handle_))
        .WillOnce(Return(video_local_handle_));
    EXPECT_CALL(*this, SendMessage(audio_remote_handle_, _))
        .WillOnce(CheckInitializeDemuxerStream(audio_remote_handle_,
                                               audio_local_handle_));
    EXPECT_CALL(*this, SendMessage(video_remote_handle_, _))
        .WillOnce(CheckInitializeDemuxerStream(video_remote_handle_,
                                               video_local_handle_));
    stream_handler_.OnRpcAcquireDemuxer(audio_remote_handle_,
                                        video_remote_handle_);
  }

  ~RpcDemuxerStreamHandlerTest() override = default;

 protected:
  class MockClient : public RpcDemuxerStreamHandler::Client {
   public:
    ~MockClient() override = default;

    MOCK_METHOD1(OnNewAudioConfig, void(media::AudioDecoderConfig));
    MOCK_METHOD1(OnNewVideoConfig, void(media::VideoDecoderConfig));
  };

  MOCK_METHOD2(SendMessage,
               void(openscreen::cast::RpcMessenger::Handle,
                    std::unique_ptr<openscreen::cast::RpcMessage>));

  MOCK_METHOD0(GetHandle, openscreen::cast::RpcMessenger::Handle());

  MOCK_METHOD1(OnBitstreamConverterEnabled, void(bool));

  void OnRpcBitstreamConverterEnabled(
      openscreen::cast::RpcMessenger::Handle handle,
      bool success) {
    static_cast<media::cast::RpcDemuxerStreamCBMessageHandler*>(
        &stream_handler_)
        ->OnRpcEnableBitstreamConverterCallback(handle, success);
  }

  void OnRpcInitializeCallback(
      openscreen::cast::RpcMessenger::Handle handle,
      std::optional<media::AudioDecoderConfig> audio_config,
      std::optional<media::VideoDecoderConfig> video_config) {
    static_cast<media::cast::RpcDemuxerStreamCBMessageHandler*>(
        &stream_handler_)
        ->OnRpcInitializeCallback(handle, std::move(audio_config),
                                  std::move(video_config));
  }

  void OnRpcReadUntilCallback(
      openscreen::cast::RpcMessenger::Handle handle,
      std::optional<media::AudioDecoderConfig> audio_config,
      std::optional<media::VideoDecoderConfig> video_config,
      uint32_t total_frames_received) {
    static_cast<media::cast::RpcDemuxerStreamCBMessageHandler*>(
        &stream_handler_)
        ->OnRpcReadUntilCallback(handle, std::move(audio_config),
                                 std::move(video_config),
                                 total_frames_received);
  }

  void RequestMoreAudioBuffers() {
    auto client = stream_handler_.GetAudioClient();
    ASSERT_TRUE(!!client);
    client->OnNoBuffersAvailable();
  }

  void RequestMoreVideoBuffers() {
    auto client = stream_handler_.GetVideoClient();
    ASSERT_TRUE(!!client);
    client->OnNoBuffersAvailable();
  }

  void OnAudioError() {
    auto client = stream_handler_.GetAudioClient();
    ASSERT_TRUE(!!client);
    client->OnError();
  }

  void OnVideoError() {
    auto client = stream_handler_.GetVideoClient();
    ASSERT_TRUE(!!client);
    client->OnError();
  }

  void EnableAudioBitstreamConverter() {
    auto client = stream_handler_.GetAudioClient();
    ASSERT_TRUE(!!client);
    client->EnableBitstreamConverter(base::BindOnce(
        &RpcDemuxerStreamHandlerTest::OnBitstreamConverterEnabled,
        base::Unretained(this)));
  }

  void EnableVideoBitstreamConverter() {
    auto client = stream_handler_.GetVideoClient();
    ASSERT_TRUE(!!client);
    client->EnableBitstreamConverter(base::BindOnce(
        &RpcDemuxerStreamHandlerTest::OnBitstreamConverterEnabled,
        base::Unretained(this)));
  }

  bool IsAudioReadUntilCallPending() {
    return static_cast<RpcDemuxerStreamHandler::MessageProcessor*>(
               stream_handler_.GetAudioClient().get())
        ->is_read_until_call_pending();
  }

  bool IsAudioReadUntilCallOngoing() {
    return static_cast<RpcDemuxerStreamHandler::MessageProcessor*>(
               stream_handler_.GetAudioClient().get())
        ->is_read_until_call_ongoing();
  }

  bool IsVideoReadUntilCallPending() {
    return static_cast<RpcDemuxerStreamHandler::MessageProcessor*>(
               stream_handler_.GetVideoClient().get())
        ->is_read_until_call_pending();
  }

  bool IsVideoReadUntilCallOngoing() {
    return static_cast<RpcDemuxerStreamHandler::MessageProcessor*>(
               stream_handler_.GetVideoClient().get())
        ->is_read_until_call_ongoing();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  openscreen::cast::RpcMessenger::Handle audio_remote_handle_ = 123;
  openscreen::cast::RpcMessenger::Handle video_remote_handle_ = 456;

  openscreen::cast::RpcMessenger::Handle audio_local_handle_ = 135;
  openscreen::cast::RpcMessenger::Handle video_local_handle_ = 246;

  media::AudioDecoderConfig test_audio_config_;
  media::VideoDecoderConfig test_video_config_;

  StrictMock<MockClient> client_;

  RpcDemuxerStreamHandler stream_handler_;
};

TEST_F(RpcDemuxerStreamHandlerTest, InitKnownAudioHandleWithAudioConfig) {
  EXPECT_CALL(client_, OnNewAudioConfig(_))
      .WillOnce([this](media::AudioDecoderConfig config) {
        EXPECT_TRUE(test_audio_config_.Matches(config));
      });
  OnRpcInitializeCallback(audio_local_handle_, test_audio_config_,
                          std::nullopt);
}

TEST_F(RpcDemuxerStreamHandlerTest, InitKnownAudioHandleWithVideoConfig) {
  OnRpcInitializeCallback(audio_local_handle_, std::nullopt,
                          test_video_config_);
}

TEST_F(RpcDemuxerStreamHandlerTest, InitKnownVideoHandleWithAudioConfig) {
  EXPECT_CALL(client_, OnNewVideoConfig(_))
      .WillOnce([this](media::VideoDecoderConfig config) {
        EXPECT_TRUE(test_video_config_.Matches(config));
      });
  OnRpcInitializeCallback(video_local_handle_, std::nullopt,
                          test_video_config_);
}

TEST_F(RpcDemuxerStreamHandlerTest, InitKnownVideoHandleWithVideoConfig) {
  OnRpcInitializeCallback(video_local_handle_, test_audio_config_,
                          std::nullopt);
}

TEST_F(RpcDemuxerStreamHandlerTest, ReadKnownAudioHandleWithAudioConfig) {
  EXPECT_CALL(client_, OnNewAudioConfig(_))
      .WillOnce([this](media::AudioDecoderConfig config) {
        EXPECT_TRUE(test_audio_config_.Matches(config));
      });
  OnRpcReadUntilCallback(audio_local_handle_, test_audio_config_, std::nullopt,
                         uint32_t{1});
}

TEST_F(RpcDemuxerStreamHandlerTest, ReadKnownAudioHandleWithVideoConfig) {
  OnRpcReadUntilCallback(audio_local_handle_, std::nullopt, test_video_config_,
                         uint32_t{1});
}

TEST_F(RpcDemuxerStreamHandlerTest, ReadKnownVideoHandleWithAudioConfig) {
  EXPECT_CALL(client_, OnNewVideoConfig(_))
      .WillOnce([this](media::VideoDecoderConfig config) {
        EXPECT_TRUE(test_video_config_.Matches(config));
      });
  OnRpcReadUntilCallback(video_local_handle_, std::nullopt, test_video_config_,
                         uint32_t{1});
}

TEST_F(RpcDemuxerStreamHandlerTest, ReadKnownVideoHandleWithVideoConfig) {
  OnRpcReadUntilCallback(video_local_handle_, test_audio_config_, std::nullopt,
                         uint32_t{1});
}

TEST_F(RpcDemuxerStreamHandlerTest, EnableBitstreamConverterKnownHandle) {
  EXPECT_CALL(*this, SendMessage(_, _))
      .WillOnce(CheckEnableBistreamConverterCall(video_remote_handle_));
  EXPECT_CALL(*this, OnBitstreamConverterEnabled(true));
  EnableVideoBitstreamConverter();
  OnRpcBitstreamConverterEnabled(video_local_handle_, true);
}

TEST_F(RpcDemuxerStreamHandlerTest, EnableBitstreamConverterUnknownHandle) {
  OnRpcBitstreamConverterEnabled(42, true);
}

TEST_F(RpcDemuxerStreamHandlerTest, RequestMoreAudioBuffers) {
  EXPECT_CALL(client_, OnNewAudioConfig(_))
      .WillOnce([this](media::AudioDecoderConfig config) {
        EXPECT_TRUE(test_audio_config_.Matches(config));
      });
  OnRpcReadUntilCallback(audio_local_handle_, test_audio_config_, std::nullopt,
                         uint32_t{12});
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Make a call with no ACK.
  EXPECT_CALL(*this, SendMessage(_, _))
      .WillOnce(
          CheckReadUntilCall(audio_remote_handle_, audio_local_handle_, 12));
  RequestMoreAudioBuffers();
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_FALSE(IsAudioReadUntilCallPending());
  EXPECT_TRUE(IsAudioReadUntilCallOngoing());

  // Callback with new config following the ACK.
  EXPECT_CALL(client_, OnNewAudioConfig(_))
      .WillOnce([this](media::AudioDecoderConfig config) {
        EXPECT_TRUE(test_audio_config_.Matches(config));
      });
  OnRpcReadUntilCallback(audio_local_handle_, test_audio_config_, std::nullopt,
                         uint32_t{42});
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_FALSE(IsAudioReadUntilCallPending());
  EXPECT_FALSE(IsAudioReadUntilCallOngoing());

  // Request is blocked until minimum time has passed.
  RequestMoreAudioBuffers();
  task_environment_.RunUntilIdle();

  // Execute request with 2 retries.
  EXPECT_CALL(*this, SendMessage(_, _))
      .Times(3)
      .WillRepeatedly(
          CheckReadUntilCall(audio_remote_handle_, audio_local_handle_, 42));
  task_environment_.FastForwardBy(base::Milliseconds(1250));
  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_FALSE(IsAudioReadUntilCallPending());
  EXPECT_TRUE(IsAudioReadUntilCallOngoing());

  // Queue up an extra call.
  RequestMoreAudioBuffers();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(IsAudioReadUntilCallPending());
  EXPECT_TRUE(IsAudioReadUntilCallOngoing());

  // Call a second time when |is_read_until_call_pending_| is set.
  EXPECT_CALL(*this, SendMessage(_, _))
      .WillOnce(
          CheckReadUntilCall(audio_remote_handle_, audio_local_handle_, 60));
  OnRpcReadUntilCallback(audio_local_handle_, std::nullopt, std::nullopt,
                         uint32_t{60});
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(IsAudioReadUntilCallPending());
  EXPECT_TRUE(IsAudioReadUntilCallOngoing());
}

TEST_F(RpcDemuxerStreamHandlerTest, RequestMoreVideoBuffers) {
  EXPECT_CALL(client_, OnNewVideoConfig(_))
      .WillOnce([this](media::VideoDecoderConfig config) {
        EXPECT_TRUE(test_video_config_.Matches(config));
      });
  OnRpcReadUntilCallback(video_local_handle_, std::nullopt, test_video_config_,
                         uint32_t{12});
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Make a call with no ACK.
  EXPECT_CALL(*this, SendMessage(_, _))
      .WillOnce(
          CheckReadUntilCall(video_remote_handle_, video_local_handle_, 12));
  RequestMoreVideoBuffers();
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_FALSE(IsVideoReadUntilCallPending());
  EXPECT_TRUE(IsVideoReadUntilCallOngoing());

  // Callback with new config following the ACK.
  EXPECT_CALL(client_, OnNewVideoConfig(_))
      .WillOnce([this](media::VideoDecoderConfig config) {
        EXPECT_TRUE(test_video_config_.Matches(config));
      });
  OnRpcReadUntilCallback(video_local_handle_, std::nullopt, test_video_config_,
                         uint32_t{42});
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_FALSE(IsVideoReadUntilCallPending());
  EXPECT_FALSE(IsVideoReadUntilCallOngoing());

  // Request is blocked until minimum time has passed.
  RequestMoreVideoBuffers();
  task_environment_.RunUntilIdle();

  // Execute request with 2 retries.
  EXPECT_CALL(*this, SendMessage(_, _))
      .Times(3)
      .WillRepeatedly(
          CheckReadUntilCall(video_remote_handle_, video_local_handle_, 42));
  task_environment_.FastForwardBy(base::Milliseconds(1250));
  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_FALSE(IsVideoReadUntilCallPending());
  EXPECT_TRUE(IsVideoReadUntilCallOngoing());

  // Queue up an extra call.
  RequestMoreVideoBuffers();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(IsVideoReadUntilCallPending());
  EXPECT_TRUE(IsVideoReadUntilCallOngoing());

  // Call a second time when |is_read_until_call_pending_| is set.
  EXPECT_CALL(*this, SendMessage(_, _))
      .WillOnce(
          CheckReadUntilCall(video_remote_handle_, video_local_handle_, 60));
  OnRpcReadUntilCallback(video_local_handle_, std::nullopt, std::nullopt,
                         uint32_t{60});
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(IsVideoReadUntilCallPending());
  EXPECT_TRUE(IsVideoReadUntilCallOngoing());
}

TEST_F(RpcDemuxerStreamHandlerTest, OnAudioError) {
  EXPECT_CALL(*this, SendMessage(_, _))
      .WillOnce(CheckOnErrorCall(audio_remote_handle_));
  OnAudioError();
}

TEST_F(RpcDemuxerStreamHandlerTest, OnVideoError) {
  EXPECT_CALL(*this, SendMessage(_, _))
      .WillOnce(CheckOnErrorCall(video_remote_handle_));
  OnVideoError();
}

TEST_F(RpcDemuxerStreamHandlerTest, OnEnableAudioBitstreamConverter) {
  EXPECT_CALL(*this, SendMessage(_, _))
      .WillOnce(CheckEnableBistreamConverterCall(audio_remote_handle_));
  EnableAudioBitstreamConverter();

  EXPECT_CALL(*this, OnBitstreamConverterEnabled(true));
  stream_handler_.OnRpcBitstreamConverterEnabled(audio_local_handle_, true);
}

TEST_F(RpcDemuxerStreamHandlerTest, OnEnableVideoBitstreamConverter) {
  EXPECT_CALL(*this, SendMessage(_, _))
      .WillOnce(CheckEnableBistreamConverterCall(video_remote_handle_));
  EnableVideoBitstreamConverter();

  EXPECT_CALL(*this, OnBitstreamConverterEnabled(false));
  stream_handler_.OnRpcBitstreamConverterEnabled(video_local_handle_, false);
}

}  // namespace cast_streaming::remoting
