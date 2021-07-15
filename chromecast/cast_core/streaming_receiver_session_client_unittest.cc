// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/streaming_receiver_session_client.h"

#include "base/test/task_environment.h"
#include "chromecast/shared/platform_info_serializer.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "components/cast_streaming/public/mojom/cast_streaming_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace chromecast {
namespace {

class MockReceiverSession : public cast_streaming::ReceiverSession {
 public:
  ~MockReceiverSession() override = default;

  MOCK_METHOD1(SetCastStreamingReceiver,
               void(mojo::AssociatedRemote<::mojom::CastStreamingReceiver>));
};

class MockStreamingReceiverSessionHandler
    : public StreamingReceiverSessionClient::Handler {
 public:
  ~MockStreamingReceiverSessionHandler() override = default;

  MOCK_METHOD0(OnStreamingSessionStarted, void());
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(StartAvSettingsQuery,
               void(std::unique_ptr<cast_api_bindings::MessagePort>));
};

}  // namespace

class StreamingReceiverSessionClientTest : public testing::Test {
 public:
  StreamingReceiverSessionClientTest() {
    auto receiver_session = std::make_unique<StrictMock<MockReceiverSession>>();
    receiver_session_ = receiver_session.get();
    EXPECT_CALL(handler_, StartAvSettingsQuery(_));

    // Note: Can't use make_unique<> because the private ctor is needed.
    auto* client = new StreamingReceiverSessionClient(
        base::BindOnce(
            &StreamingReceiverSessionClientTest::CreateReceiverSession,
            base::Unretained(this), std::move(receiver_session)),
        &handler_);
    receiver_session_client_.reset(client);
  }

 protected:
  // Needed due to the complexity of mocking a CastWebContents to call
  // LaunchStreamingReceiver directly.
  void SetStarted() {
    receiver_session_client_->has_streaming_started_ = true;
    ASSERT_TRUE(receiver_session_client_->has_streaming_started());
  }

  bool PostMessage(base::StringPiece message) {
    return receiver_session_client_->OnMessage(message, {});
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  StrictMock<MockStreamingReceiverSessionHandler> handler_;
  StrictMock<MockReceiverSession>* receiver_session_;
  std::unique_ptr<StreamingReceiverSessionClient> receiver_session_client_;

  // Set when the session is launched.
  cast_streaming::ReceiverSession::AVConstraints session_constraints_;

 private:
  std::unique_ptr<cast_streaming::ReceiverSession> CreateReceiverSession(
      std::unique_ptr<cast_streaming::ReceiverSession> ptr,
      cast_streaming::ReceiverSession::AVConstraints constraints) {
    session_constraints_ = constraints;
    return ptr;
  }
};

TEST_F(StreamingReceiverSessionClientTest, OnSingleValidMessageEmpty) {
  PlatformInfoSerializer serializer;
  EXPECT_TRUE(PostMessage(serializer.ToJson()));
  receiver_session_client_->LaunchStreamingReceiver(nullptr);

  cast_streaming::ReceiverSession::AVConstraints defaults;
  EXPECT_TRUE(defaults.IsSupersetOf(session_constraints_));
  EXPECT_TRUE(session_constraints_.IsSupersetOf(defaults));
}

TEST_F(StreamingReceiverSessionClientTest, OnSingleValidMessageNoCodecs) {
  PlatformInfoSerializer serializer;
  serializer.SetMaxChannels(9);
  EXPECT_TRUE(PostMessage(serializer.ToJson()));
  receiver_session_client_->LaunchStreamingReceiver(nullptr);

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
  EXPECT_TRUE(PostMessage(serializer.ToJson()));
  receiver_session_client_->LaunchStreamingReceiver(nullptr);

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
  EXPECT_EQ(video_codecs.size(), size_t{2});
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
  EXPECT_TRUE(PostMessage(serializer.ToJson()));
  SetStarted();
  serializer.SetMaxChannels(8);
  EXPECT_CALL(handler_, OnError());
  EXPECT_FALSE(PostMessage(serializer.ToJson()));
}

}  // namespace chromecast
