// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/streaming_receiver_session_client.h"

#include "base/test/task_environment.h"
#include "chromecast/browser/test/mock_cast_web_view.h"
#include "chromecast/cast_core/runtime/browser/streaming_controller.h"
#include "chromecast/shared/platform_info_serializer.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "components/cast_streaming/public/mojom/renderer_controller.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace chromecast {
namespace {

class MockStreamingController : public StreamingController {
 public:
  ~MockStreamingController() override = default;

  MOCK_METHOD2(
      InitializeReceiverSession,
      void(std::unique_ptr<cast_streaming::ReceiverSession::AVConstraints>,
           cast_streaming::ReceiverSession::Client*));
  MOCK_METHOD1(StartPlaybackAsync,
               void(StreamingController::PlaybackStartedCB));
};

class MockStreamingReceiverSessionHandler
    : public StreamingReceiverSessionClient::Handler {
 public:
  ~MockStreamingReceiverSessionHandler() override = default;

  MOCK_METHOD0(OnStreamingSessionStarted, void());
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(StartAvSettingsQuery,
               void(std::unique_ptr<cast_api_bindings::MessagePort>));
  MOCK_METHOD2(OnResolutionChanged,
               void(const gfx::Rect&, const ::media::VideoTransformation&));
};

}  // namespace

class StreamingReceiverSessionClientTest : public testing::Test {
 public:
  StreamingReceiverSessionClientTest() {
    // NOTE: Required to ensure this test suite isn't affected by use of this
    // static function elsewhere in the codebase's tests.
    cast_streaming::ClearNetworkContextGetter();

    auto streaming_controller =
        std::make_unique<StrictMock<MockStreamingController>>();
    streaming_controller_ = streaming_controller.get();
    EXPECT_CALL(handler_, StartAvSettingsQuery(_));

    // Note: Can't use make_unique<> because the private ctor is needed.
    auto* client = new StreamingReceiverSessionClient(
        task_environment_.GetMainThreadTaskRunner(),
        base::BindRepeating(
            []() -> network::mojom::NetworkContext* { return nullptr; }),
        std::move(streaming_controller), &handler_, true, true);
    receiver_session_client_.reset(client);

    ON_CALL(*streaming_controller_, InitializeReceiverSession(_, _))
        .WillByDefault(Invoke(
            this, &StreamingReceiverSessionClientTest::CreateReceiverSession));
  }

  ~StreamingReceiverSessionClientTest() override {
    ResetMessagePort();
    task_environment_.FastForwardBy(
        StreamingReceiverSessionClient::kMaxAVSettingsWaitTime);
  }

 protected:
  bool PostMessage(base::StringPiece message) {
    return receiver_session_client_->OnMessage(message, {});
  }

  // When calling task_environment_.FastForwardBy(), OnPipeError() gets called.
  // Resetting the pipe is cleaner than passing in a base::OnceCallback() to
  // create the MessagePort pair.
  void ResetMessagePort() { receiver_session_client_->message_port_.reset(); }

  void CreateReceiverSession(
      std::unique_ptr<cast_streaming::ReceiverSession::AVConstraints>
          constraints,
      cast_streaming::ReceiverSession::Client* client) {
    session_constraints_ = *constraints;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  StrictMock<MockStreamingReceiverSessionHandler> handler_;
  StrictMock<MockStreamingController>* streaming_controller_;
  std::unique_ptr<StreamingReceiverSessionClient> receiver_session_client_;

  // Set when the session is launched.
  cast_streaming::ReceiverSession::AVConstraints session_constraints_;
};

TEST_F(StreamingReceiverSessionClientTest, OnSingleValidMessageEmpty) {
  PlatformInfoSerializer serializer;
  EXPECT_FALSE(receiver_session_client_->has_received_av_settings());
  EXPECT_CALL(*streaming_controller_, InitializeReceiverSession(_, _));
  EXPECT_TRUE(PostMessage(serializer.Serialize()));
  EXPECT_TRUE(receiver_session_client_->has_received_av_settings());

  cast_streaming::ReceiverSession::AVConstraints defaults;
  EXPECT_TRUE(defaults.IsSupersetOf(session_constraints_));
  EXPECT_TRUE(session_constraints_.IsSupersetOf(defaults));
}

TEST_F(StreamingReceiverSessionClientTest, OnSingleValidMessageNoCodecs) {
  PlatformInfoSerializer serializer;
  serializer.SetMaxChannels(9);
  EXPECT_FALSE(receiver_session_client_->has_received_av_settings());
  EXPECT_CALL(*streaming_controller_, InitializeReceiverSession(_, _));
  EXPECT_TRUE(PostMessage(serializer.Serialize()));
  EXPECT_TRUE(receiver_session_client_->has_received_av_settings());

  ASSERT_EQ(session_constraints_.audio_limits.size(), size_t{1});
  auto& limit = session_constraints_.audio_limits.back();
  EXPECT_TRUE(limit.applies_to_all_codecs);
  EXPECT_EQ(limit.max_channels, 9);
}

TEST_F(StreamingReceiverSessionClientTest, OnSingleValidMessageWithCodecs) {
  PlatformInfoSerializer serializer;
  std::vector<PlatformInfoSerializer::AudioCodecInfo> audio_infos;
  audio_infos.push_back(PlatformInfoSerializer::AudioCodecInfo{
      media::AudioCodec::kCodecOpus, media::SampleFormat::kSampleFormatU8, 123,
      456});
  audio_infos.push_back(PlatformInfoSerializer::AudioCodecInfo{
      media::AudioCodec::kCodecOpus, media::SampleFormat::kSampleFormatS24, 123,
      456});
  audio_infos.push_back(PlatformInfoSerializer::AudioCodecInfo{
      media::AudioCodec::kCodecMP3, media::SampleFormat::kSampleFormatU8, 42,
      42});
  audio_infos.push_back(PlatformInfoSerializer::AudioCodecInfo{
      media::AudioCodec::kCodecMP3, media::SampleFormat::kSampleFormatS24, 42,
      42});
  std::vector<PlatformInfoSerializer::VideoCodecInfo> video_infos;
  video_infos.push_back(PlatformInfoSerializer::VideoCodecInfo{
      media::VideoCodec::kCodecVP9, media::VideoProfile::kVP9Profile1});
  video_infos.push_back(PlatformInfoSerializer::VideoCodecInfo{
      media::VideoCodec::kCodecVP9, media::VideoProfile::kVP9Profile2});
  video_infos.push_back(PlatformInfoSerializer::VideoCodecInfo{
      media::VideoCodec::kCodecAV1, media::VideoProfile::kAV1ProfilePro});
  video_infos.push_back(PlatformInfoSerializer::VideoCodecInfo{
      media::VideoCodec::kCodecVP8, media::VideoProfile::kVP8ProfileAny});

  serializer.SetSupportedAudioCodecs(std::move(audio_infos));
  serializer.SetSupportedVideoCodecs(std::move(video_infos));
  EXPECT_FALSE(receiver_session_client_->has_received_av_settings());
  EXPECT_CALL(*streaming_controller_, InitializeReceiverSession(_, _));
  EXPECT_TRUE(PostMessage(serializer.Serialize()));
  EXPECT_TRUE(receiver_session_client_->has_received_av_settings());

  ASSERT_GE(session_constraints_.audio_codecs.size(), size_t{1});
  EXPECT_EQ(session_constraints_.audio_codecs.size(), size_t{1});
  EXPECT_EQ(session_constraints_.audio_codecs[0],
            openscreen::cast::AudioCodec::kOpus);
  ASSERT_GE(session_constraints_.audio_limits.size(), size_t{1});
  EXPECT_EQ(session_constraints_.audio_limits.size(), size_t{1});
  auto& limit = session_constraints_.audio_limits.back();
  EXPECT_FALSE(limit.applies_to_all_codecs);
  EXPECT_EQ(limit.codec, openscreen::cast::AudioCodec::kOpus);
  EXPECT_EQ(limit.max_sample_rate, 123);
  EXPECT_EQ(limit.max_channels, 456);

  auto video_codecs = session_constraints_.video_codecs;
  EXPECT_EQ(video_codecs.size(), size_t{3});
  EXPECT_NE(std::find(video_codecs.begin(), video_codecs.end(),
                      openscreen::cast::VideoCodec::kVp9),
            video_codecs.end());
  EXPECT_NE(std::find(video_codecs.begin(), video_codecs.end(),
                      openscreen::cast::VideoCodec::kVp8),
            video_codecs.end());
  EXPECT_TRUE(session_constraints_.video_limits.empty());
}

TEST_F(StreamingReceiverSessionClientTest, OnCapabilitiesDecrease) {
  PlatformInfoSerializer serializer;
  serializer.SetMaxChannels(9);
  EXPECT_FALSE(receiver_session_client_->has_received_av_settings());
  EXPECT_CALL(*streaming_controller_, InitializeReceiverSession(_, _));
  EXPECT_TRUE(PostMessage(serializer.Serialize()));
  EXPECT_TRUE(receiver_session_client_->has_received_av_settings());

  EXPECT_CALL(*streaming_controller_, StartPlaybackAsync(_))
      .WillOnce([](StreamingController::PlaybackStartedCB cb) {
        std::move(cb).Run();
      });
  EXPECT_CALL(handler_, OnStreamingSessionStarted());
  receiver_session_client_->LaunchStreamingReceiverAsync();

  serializer.SetMaxChannels(8);
  EXPECT_CALL(handler_, OnError());
  EXPECT_FALSE(PostMessage(serializer.Serialize()));
}

TEST_F(StreamingReceiverSessionClientTest, FailureWhenNoAvSettingsAfterLaunch) {
  EXPECT_FALSE(receiver_session_client_->is_streaming_launch_pending());
  EXPECT_FALSE(receiver_session_client_->has_streaming_launched());
  EXPECT_FALSE(receiver_session_client_->has_received_av_settings());

  EXPECT_CALL(*streaming_controller_, StartPlaybackAsync(_));
  receiver_session_client_->LaunchStreamingReceiverAsync();

  EXPECT_TRUE(receiver_session_client_->is_streaming_launch_pending());
  EXPECT_FALSE(receiver_session_client_->has_streaming_launched());
  EXPECT_FALSE(receiver_session_client_->has_received_av_settings());

  ResetMessagePort();
  EXPECT_CALL(handler_, OnError());
  task_environment_.FastForwardBy(
      StreamingReceiverSessionClient::kMaxAVSettingsWaitTime);
}

}  // namespace chromecast
