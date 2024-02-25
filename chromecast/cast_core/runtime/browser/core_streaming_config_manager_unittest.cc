// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/core_streaming_config_manager.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/test/task_environment.h"
#include "chromecast/shared/platform_info_serializer.h"
#include "components/cast_receiver/browser/public/runtime_application.h"
#include "components/cast_receiver/common/public/status.h"
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

class MockConfigObserver
    : public cast_receiver::StreamingConfigManager::ConfigObserver {
 public:
  MOCK_METHOD1(OnStreamingConfigSet,
               void(const cast_streaming::ReceiverConfig&));
};

}  // namespace

class CoreStreamingConfigManagerTest : public testing::Test {
 public:
  CoreStreamingConfigManagerTest()
      : streaming_config_manager_(
            base::BindOnce(&CoreStreamingConfigManagerTest::FailOnError,
                           base::Unretained(this))) {
    streaming_config_manager_.AddConfigObserver(observer_);
  }

  ~CoreStreamingConfigManagerTest() override { ResetMessagePort(); }

 protected:
  bool PostMessage(std::string_view message) {
    return streaming_config_manager_.OnMessage(message, {});
  }

  // When calling task_environment_.FastForwardBy(), OnPipeError() gets called.
  // Resetting the pipe is cleaner than passing in a base::OnceCallback() to
  // create the MessagePort pair.
  void ResetMessagePort() { streaming_config_manager_.message_port_.reset(); }

  void FailOnError(cast_receiver::Status status) { FAIL() << status; }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  CoreStreamingConfigManager streaming_config_manager_;
  testing::StrictMock<MockConfigObserver> observer_;
};

TEST_F(CoreStreamingConfigManagerTest, OnSingleValidMessageEmpty) {
  PlatformInfoSerializer serializer;
  EXPECT_FALSE(streaming_config_manager_.has_config());
  EXPECT_CALL(observer_, OnStreamingConfigSet(_));
  EXPECT_TRUE(PostMessage(serializer.Serialize()));
  EXPECT_TRUE(streaming_config_manager_.has_config());
}

TEST_F(CoreStreamingConfigManagerTest, OnSingleValidMessageNoCodecs) {
  PlatformInfoSerializer serializer;
  serializer.SetMaxChannels(2);
  EXPECT_FALSE(streaming_config_manager_.has_config());
  EXPECT_CALL(observer_, OnStreamingConfigSet(_));
  EXPECT_TRUE(PostMessage(serializer.Serialize()));
  EXPECT_TRUE(streaming_config_manager_.has_config());

  auto config = streaming_config_manager_.config();
  ASSERT_EQ(config.audio_limits.size(), size_t{1});
  auto& limit = config.audio_limits.back();
  EXPECT_EQ(limit.codec, std::nullopt);
  EXPECT_EQ(limit.channel_layout, ::media::CHANNEL_LAYOUT_STEREO);
}

TEST_F(CoreStreamingConfigManagerTest, OnSingleValidMessageWithCodecs) {
  PlatformInfoSerializer serializer;
  std::vector<PlatformInfoSerializer::AudioCodecInfo> audio_infos;
  audio_infos.push_back(PlatformInfoSerializer::AudioCodecInfo{
      media::AudioCodec::kCodecOpus, media::SampleFormat::kSampleFormatU8, 123,
      2});
  audio_infos.push_back(PlatformInfoSerializer::AudioCodecInfo{
      media::AudioCodec::kCodecOpus, media::SampleFormat::kSampleFormatS24, 123,
      2});
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
  EXPECT_FALSE(streaming_config_manager_.has_config());
  EXPECT_CALL(observer_, OnStreamingConfigSet(_));
  EXPECT_TRUE(PostMessage(serializer.Serialize()));
  EXPECT_TRUE(streaming_config_manager_.has_config());

  const auto& config = streaming_config_manager_.config();
  ASSERT_GE(config.audio_codecs.size(), size_t{1});
  EXPECT_EQ(config.audio_codecs.size(), size_t{1});
  EXPECT_EQ(config.audio_codecs[0], ::media::AudioCodec::kOpus);
  ASSERT_GE(config.audio_limits.size(), size_t{1});
  EXPECT_EQ(config.audio_limits.size(), size_t{1});
  auto& limit = config.audio_limits.back();
  ASSERT_TRUE(limit.codec.has_value());
  EXPECT_EQ(limit.codec.value(), ::media::AudioCodec::kOpus);
  EXPECT_EQ(limit.max_sample_rate, 123);

  auto video_codecs = config.video_codecs;
  EXPECT_EQ(video_codecs.size(), size_t{3});
  EXPECT_TRUE(base::Contains(video_codecs, ::media::VideoCodec::kVP9));
  EXPECT_TRUE(base::Contains(video_codecs, ::media::VideoCodec::kVP8));
  EXPECT_TRUE(config.video_limits.empty());
}

}  // namespace chromecast
