// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_pipeline_backend_starboard.h"

#include "base/test/task_environment.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"
#include "chromecast/starboard/media/media/mock_starboard_api_wrapper.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::DoubleEq;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArg;

// Takes a StarboardPlayerCreationParam* argument and ensures that its max video
// capabilities specify streaming=1.
MATCHER(CreationParamHasStreamingEnabled, "") {
  if (!arg) {
    *result_listener << "the StarboardPlayerCreationParam* is null";
    return false;
  }
  return strcmp(arg->video_sample_info.max_video_capabilities, "streaming=1") ==
         0;
}

// A mock delegate that can be passed to decoders.
class MockDelegate : public MediaPipelineBackend::Decoder::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void, OnPushBufferComplete, (BufferStatus status), (override));
  MOCK_METHOD(void, OnEndOfStream, (), (override));
  MOCK_METHOD(void, OnDecoderError, (), (override));
  MOCK_METHOD(void,
              OnKeyStatusChanged,
              (const std::string& key_id,
               CastKeyStatus key_status,
               uint32_t system_code),
              (override));
  MOCK_METHOD(void, OnVideoResolutionChanged, (const Size& size), (override));
};

// Returns a simple AudioConfig.
AudioConfig GetBasicAudioConfig() {
  AudioConfig config;

  config.codec = AudioCodec::kCodecMP3;
  config.channel_layout = ChannelLayout::STEREO;
  config.sample_format = SampleFormat::kSampleFormatF32;
  config.bytes_per_channel = 4;
  config.channel_number = 2;
  config.samples_per_second = 44100;
  config.encryption_scheme = EncryptionScheme::kUnencrypted;

  return config;
}

// Returns a simple VideoConfig.
VideoConfig GetBasicVideoConfig() {
  VideoConfig config;

  config.codec = VideoCodec::kCodecH264;
  config.encryption_scheme = EncryptionScheme::kUnencrypted;
  config.width = 123;
  config.height = 456;

  return config;
}

// A test fixture is used to manage the global mock state and to handle the
// lifetime of the SingleThreadTaskEnvironment.
class MediaPipelineBackendStarboardTest : public ::testing::Test {
 protected:
  MediaPipelineBackendStarboardTest()
      : starboard_(std::make_unique<MockStarboardApiWrapper>()),
        device_params_(
            /*task_runner_in=*/nullptr,
            AudioContentType::kMedia,
            /*device_id_in=*/"id") {
    // Sets up default behavior for the mock functions that return values, so
    // that tests that do not care about this functionality can ignore them.
    ON_CALL(*starboard_, CreatePlayer).WillByDefault(Return(&fake_player_));
    ON_CALL(*starboard_, EnsureInitialized).WillByDefault(Return(true));
    ON_CALL(*starboard_, SetPlaybackRate).WillByDefault(Return(true));
  }

  ~MediaPipelineBackendStarboardTest() override = default;

  // This should be destructed last.
  base::test::SingleThreadTaskEnvironment task_environment_;
  // This will be passed to the MediaPipelineBackendStarboard, and all calls to
  // Starboard will go through it. Thus, we can mock out those calls.
  std::unique_ptr<MockStarboardApiWrapper> starboard_;
  // Since SbPlayer is just an opaque blob to the MPB, we will simply use an int
  // to represent it.
  int fake_player_ = 1;
  StarboardVideoPlane video_plane_;
  MediaPipelineDeviceParams device_params_;
};

TEST_F(MediaPipelineBackendStarboardTest, InitializesSuccessfully) {
  EXPECT_CALL(*starboard_, EnsureInitialized).WillOnce(Return(true));
  EXPECT_CALL(*starboard_, CreatePlayer).WillOnce(Return(&fake_player_));

  MediaPipelineBackendStarboard backend(device_params_, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));

  EXPECT_TRUE(backend.Initialize());
}

TEST_F(MediaPipelineBackendStarboardTest,
       SetsMaxVideoCapabilitiesToStreamingForIgnorePtsSyncType) {
  EXPECT_CALL(*starboard_, EnsureInitialized).WillOnce(Return(true));
  EXPECT_CALL(*starboard_, CreatePlayer(CreationParamHasStreamingEnabled(), _))
      .WillOnce(Return(&fake_player_));

  MediaPipelineDeviceParams device_params = device_params_;
  device_params.sync_type =
      MediaPipelineDeviceParams::MediaSyncType::kModeIgnorePts;
  MediaPipelineBackendStarboard backend(device_params, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));

  // We need to create a video decoder in order for video params to be set when
  // creating the SbPlayer.
  MediaPipelineBackend::VideoDecoder* video_decoder =
      backend.CreateVideoDecoder();
  ASSERT_THAT(video_decoder, NotNull());
  video_decoder->SetConfig(VideoConfig());

  EXPECT_TRUE(backend.Initialize());
}

TEST_F(MediaPipelineBackendStarboardTest,
       DoesNotSetMaxVideoCapabilitiesToStreamingForSyncPtsSyncType) {
  EXPECT_CALL(*starboard_, EnsureInitialized).WillOnce(Return(true));
  EXPECT_CALL(*starboard_,
              CreatePlayer(
                  AllOf(NotNull(), Not(CreationParamHasStreamingEnabled())), _))
      .WillOnce(Return(&fake_player_));

  MediaPipelineDeviceParams device_params = device_params_;
  device_params.sync_type =
      MediaPipelineDeviceParams::MediaSyncType::kModeSyncPts;
  MediaPipelineBackendStarboard backend(device_params, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));

  // We need to create a video decoder in order for video params to be set when
  // creating the SbPlayer.
  MediaPipelineBackend::VideoDecoder* video_decoder =
      backend.CreateVideoDecoder();
  ASSERT_THAT(video_decoder, NotNull());
  video_decoder->SetConfig(VideoConfig());

  EXPECT_TRUE(backend.Initialize());
}

TEST_F(MediaPipelineBackendStarboardTest,
       HandlesGeometryChangeAfterPlayerCreation) {
  EXPECT_CALL(*starboard_, CreatePlayer).WillOnce(Return(&fake_player_));
  EXPECT_CALL(*starboard_, SetPlayerBounds(&fake_player_, 0, 1, 2, 1920, 1080));

  MediaPipelineBackendStarboard backend(device_params_, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));

  MediaPipelineBackend::VideoDecoder* video_decoder =
      backend.CreateVideoDecoder();

  // The video's width and height match the display's aspect ratio (set above).
  // Thus, the player's bound should be set to the full screen, 1920x1080.
  VideoConfig config = GetBasicVideoConfig();
  config.width = 1280;
  config.height = 720;
  video_decoder->SetConfig(config);

  EXPECT_TRUE(backend.Initialize());

  video_plane_.SetGeometry(RectF(1, 2, 1920, 1080),
                           StarboardVideoPlane::Transform::TRANSFORM_NONE);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaPipelineBackendStarboardTest,
       HandlesGeometryChangeBeforePlayerCreation) {
  EXPECT_CALL(*starboard_, CreatePlayer).WillOnce(Return(&fake_player_));
  EXPECT_CALL(*starboard_, SetPlayerBounds(&fake_player_, 0, 3, 4, 1920, 1080));

  MediaPipelineBackendStarboard backend(device_params_, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));

  MediaPipelineBackend::VideoDecoder* video_decoder =
      backend.CreateVideoDecoder();

  // The video's width and height match the display's aspect ratio (set above).
  // Thus, the player's bound should be set to the full screen, 1920x1080.
  VideoConfig config = GetBasicVideoConfig();
  config.width = 1280;
  config.height = 720;
  video_decoder->SetConfig(config);

  video_plane_.SetGeometry(RectF(3, 4, 1920, 1080),
                           StarboardVideoPlane::Transform::TRANSFORM_NONE);
  task_environment_.RunUntilIdle();

  // Calling Initialize should trigger the call to set the player's bounds.
  EXPECT_TRUE(backend.Initialize());
}

TEST_F(MediaPipelineBackendStarboardTest, DestroysPlayerOnDestruction) {
  EXPECT_CALL(*starboard_, DestroyPlayer(&fake_player_)).Times(1);

  MediaPipelineBackendStarboard backend(device_params_, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));
  EXPECT_TRUE(backend.Initialize());
}

TEST_F(MediaPipelineBackendStarboardTest,
       DoesNotCallDestroyPlayerIfNotInitialized) {
  EXPECT_CALL(*starboard_, DestroyPlayer).Times(0);

  MediaPipelineBackendStarboard backend(device_params_, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));
}

TEST_F(MediaPipelineBackendStarboardTest, SeeksToBeginningOnStart) {
  constexpr int64_t start_time = 0;

  EXPECT_CALL(*starboard_, SeekTo(&fake_player_, start_time, _)).Times(1);
  EXPECT_CALL(*starboard_, SetPlaybackRate(&fake_player_, DoubleEq(1.0)))
      .Times(1);

  MediaPipelineBackendStarboard backend(device_params_, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));
  CHECK(backend.Initialize());
  EXPECT_TRUE(backend.Start(start_time));
}

TEST_F(MediaPipelineBackendStarboardTest, SeeksToMidPointOnStart) {
  constexpr int64_t start_time = 10;

  EXPECT_CALL(*starboard_, SeekTo(&fake_player_, start_time, _)).Times(1);
  EXPECT_CALL(*starboard_, SetPlaybackRate(&fake_player_, DoubleEq(1.0)))
      .Times(1);

  MediaPipelineBackendStarboard backend(device_params_, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));
  CHECK(backend.Initialize());
  EXPECT_TRUE(backend.Start(start_time));
}

TEST_F(MediaPipelineBackendStarboardTest, SetsPlaybackRateToZeroOnPause) {
  EXPECT_CALL(*starboard_, SetPlaybackRate(&fake_player_, DoubleEq(1.0)))
      .Times(AnyNumber());
  // Pausing  playback means setting the playback rate to 0 in starboard.
  EXPECT_CALL(*starboard_, SetPlaybackRate(&fake_player_, DoubleEq(0.0)))
      .Times(1);

  MediaPipelineBackendStarboard backend(device_params_, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));
  CHECK(backend.Initialize());
  CHECK(backend.Start(0));

  EXPECT_TRUE(backend.Pause());
}

TEST_F(MediaPipelineBackendStarboardTest,
       SetsPlaybackRateToPreviousValueOnResume) {
  constexpr double playback_rate = 2.0;

  // This should be called when playback is paused.
  EXPECT_CALL(*starboard_, GetPlayerInfo)
      .WillRepeatedly(WithArg<1>([](StarboardPlayerInfo* player_info) {
        CHECK(player_info);
        player_info->playback_rate = playback_rate;
      }));

  EXPECT_CALL(*starboard_, SetPlaybackRate(&fake_player_, DoubleEq(1.0)))
      .Times(AnyNumber());
  // This should be called twice: once when we set the playback rate, and once
  // when we resume playback.
  EXPECT_CALL(*starboard_,
              SetPlaybackRate(&fake_player_, DoubleEq(playback_rate)))
      .Times(2);
  // This should be called when we pause playback.
  EXPECT_CALL(*starboard_, SetPlaybackRate(&fake_player_, DoubleEq(0.0)))
      .Times(1);

  MediaPipelineBackendStarboard backend(device_params_, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));
  CHECK(backend.Initialize());
  CHECK(backend.Start(0));

  EXPECT_TRUE(backend.SetPlaybackRate(playback_rate));
  EXPECT_TRUE(backend.Pause());
  EXPECT_TRUE(backend.Resume());
}

TEST_F(MediaPipelineBackendStarboardTest, GetsCurrentPts) {
  constexpr int64_t current_pts = 123;

  EXPECT_CALL(*starboard_, GetPlayerInfo)
      .WillRepeatedly(WithArg<1>([](StarboardPlayerInfo* player_info) {
        CHECK(player_info);
        player_info->current_media_timestamp_micros = current_pts;
        player_info->playback_rate = 1.0;
      }));

  MediaPipelineBackendStarboard backend(device_params_, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));
  CHECK(backend.Initialize());
  CHECK(backend.Start(0));

  EXPECT_EQ(backend.GetCurrentPts(), current_pts);
}

TEST_F(MediaPipelineBackendStarboardTest,
       CallsDelegateEndOfStreamWhenStarboardReportsEoS) {
  const StarboardPlayerCallbackHandler* callback_handler = nullptr;
  EXPECT_CALL(*starboard_, CreatePlayer)
      .WillOnce(DoAll(SaveArg<1>(&callback_handler), Return(&fake_player_)));

  MediaPipelineBackendStarboard backend(device_params_, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));

  MediaPipelineBackend::AudioDecoder* audio_decoder =
      backend.CreateAudioDecoder();
  MediaPipelineBackend::VideoDecoder* video_decoder =
      backend.CreateVideoDecoder();

  ASSERT_THAT(audio_decoder, NotNull());
  ASSERT_THAT(video_decoder, NotNull());
  audio_decoder->SetConfig(GetBasicAudioConfig());
  video_decoder->SetConfig(GetBasicVideoConfig());

  MockDelegate audio_delegate;
  MockDelegate video_delegate;
  EXPECT_CALL(audio_delegate, OnEndOfStream).Times(1);
  EXPECT_CALL(video_delegate, OnEndOfStream).Times(1);

  audio_decoder->SetDelegate(&audio_delegate);
  video_decoder->SetDelegate(&video_delegate);

  CHECK(backend.Initialize());
  CHECK(backend.Start(0));

  ASSERT_THAT(callback_handler, NotNull());

  // This should trigger the calls to audio_delegate.OnEndOfStream() and
  // video_delegate.OnEndOfStream().
  callback_handler->player_status_fn(&fake_player_, callback_handler->context,
                                     kStarboardPlayerStateEndOfStream,
                                     /*ticket=*/1);
  Mock::VerifyAndClearExpectations(&audio_delegate);
  Mock::VerifyAndClearExpectations(&video_delegate);
}

TEST_F(MediaPipelineBackendStarboardTest,
       DelegatesAreNotifiedOnStarboardDecoderError) {
  const StarboardPlayerCallbackHandler* callback_handler = nullptr;
  EXPECT_CALL(*starboard_, CreatePlayer)
      .WillOnce(DoAll(SaveArg<1>(&callback_handler), Return(&fake_player_)));

  MediaPipelineBackendStarboard backend(device_params_, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));

  MediaPipelineBackend::AudioDecoder* audio_decoder =
      backend.CreateAudioDecoder();
  MediaPipelineBackend::VideoDecoder* video_decoder =
      backend.CreateVideoDecoder();

  ASSERT_THAT(audio_decoder, NotNull());
  ASSERT_THAT(video_decoder, NotNull());
  audio_decoder->SetConfig(GetBasicAudioConfig());
  video_decoder->SetConfig(GetBasicVideoConfig());

  MockDelegate audio_delegate;
  MockDelegate video_delegate;
  EXPECT_CALL(audio_delegate, OnDecoderError).Times(1);
  EXPECT_CALL(video_delegate, OnDecoderError).Times(1);

  audio_decoder->SetDelegate(&audio_delegate);
  video_decoder->SetDelegate(&video_delegate);

  CHECK(backend.Initialize());
  CHECK(backend.Start(0));

  ASSERT_THAT(callback_handler, NotNull());

  // This should trigger the calls to audio_delegate.OnDecoderError() and
  // video_delegate.OnDecoderError().
  callback_handler->player_error_fn(&fake_player_, callback_handler->context,
                                    kStarboardPlayerErrorDecode,
                                    "A decoder error occurred");
  Mock::VerifyAndClearExpectations(&audio_delegate);
  Mock::VerifyAndClearExpectations(&video_delegate);
}

TEST_F(MediaPipelineBackendStarboardTest,
       DelegatesAreNotNotifiedOnUnrelatedError) {
  const StarboardPlayerCallbackHandler* callback_handler = nullptr;
  EXPECT_CALL(*starboard_, CreatePlayer)
      .WillOnce(DoAll(SaveArg<1>(&callback_handler), Return(&fake_player_)));

  MediaPipelineBackendStarboard backend(device_params_, &video_plane_);
  backend.TestOnlySetStarboardApiWrapper(std::move(starboard_));

  MediaPipelineBackend::AudioDecoder* audio_decoder =
      backend.CreateAudioDecoder();
  MediaPipelineBackend::VideoDecoder* video_decoder =
      backend.CreateVideoDecoder();

  ASSERT_THAT(audio_decoder, NotNull());
  ASSERT_THAT(video_decoder, NotNull());
  audio_decoder->SetConfig(GetBasicAudioConfig());
  video_decoder->SetConfig(GetBasicVideoConfig());

  MockDelegate audio_delegate;
  MockDelegate video_delegate;
  EXPECT_CALL(audio_delegate, OnDecoderError).Times(0);
  EXPECT_CALL(video_delegate, OnDecoderError).Times(0);

  audio_decoder->SetDelegate(&audio_delegate);
  video_decoder->SetDelegate(&video_delegate);

  CHECK(backend.Initialize());
  CHECK(backend.Start(0));

  ASSERT_THAT(callback_handler, NotNull());

  // This should NOT trigger the calls to audio_delegate.OnDecoderError() and
  // video_delegate.OnDecoderError(), since it's not a decoder error.
  callback_handler->player_error_fn(&fake_player_, callback_handler->context,
                                    kStarboardPlayerErrorCapabilityChanged,
                                    "Starboard capabilities changed");
  Mock::VerifyAndClearExpectations(&audio_delegate);
  Mock::VerifyAndClearExpectations(&video_delegate);
}

}  // namespace
}  // namespace media
}  // namespace chromecast
