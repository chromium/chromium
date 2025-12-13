// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The StarboardRenderer class does not perform much active logic. Most of its
// functionality is separated into other classes, which are stored as data
// members of StarboardRenderer. As such, these tests are more like integration
// tests, which verify that the sub-components of StarboardRenderer
// (GeometryChangeHandler, StarboardPlayerManager, etc.) all work together and
// implement the functionality expected of a ::media::Renderer.
//
// One important detail here is that StarboardRenderer does not actually do any
// rendering or decoding directly. That is done by the starboard library itself.
// Here, MockStarboardApiWrapper is used to verify that the correct data is
// being passed to starboard.

#include "chromecast/starboard/media/renderer/starboard_renderer.h"

#include <array>
#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromecast/base/metrics/mock_cast_metrics_helper.h"
#include "chromecast/media/service/mojom/video_geometry_setter.mojom.h"
#include "chromecast/media/service/video_geometry_setter_service.h"
#include "chromecast/starboard/media/cdm/starboard_drm_key_tracker.h"
#include "chromecast/starboard/media/cdm/starboard_drm_wrapper.h"
#include "chromecast/starboard/media/media/mock_starboard_api_wrapper.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/media/media/test_matchers.h"
#include "media/base/audio_codecs.h"
#include "media/base/buffering_state.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/mock_filters.h"
#include "media/base/pipeline_status.h"
#include "media/base/test_helpers.h"
#include "media/base/video_codecs.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace chromecast {
namespace media {
namespace {

using ::base::test::RunOnceCallback;
using ::media::DemuxerStream;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::DoubleEq;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArg;

// Runs any pending tasks that have been posted to the current sequence.
void RunPendingTasks() {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Returns a valid audio config with values arbitrarily set. The values will
// match the values of GetStarboardAudioConfig.
::media::AudioDecoderConfig GetChromiumAudioConfig() {
  return ::media::AudioDecoderConfig(
      ::media::AudioCodec::kAAC, ::media::SampleFormat::kSampleFormatS32,
      ::media::ChannelLayout::CHANNEL_LAYOUT_STEREO, 48000, /*extra_data=*/{},
      ::media::EncryptionScheme::kUnencrypted);
}

// Returns a valid video config with values arbitrarily set. The values will
// match the values of GetStarboardVideoConfig.
::media::VideoDecoderConfig GetChromiumVideoConfig() {
  ::media::VideoDecoderConfig video_config(
      ::media::VideoCodec::kH264, ::media::VideoCodecProfile::H264PROFILE_HIGH,
      ::media::VideoDecoderConfig::AlphaMode::kIsOpaque,
      ::media::VideoColorSpace(1, 1, 1, gfx::ColorSpace::RangeID::LIMITED),
      ::media::VideoTransformation(), gfx::Size(3840, 2160),
      gfx::Rect(0, 0, 3838, 2121), gfx::Size(1920, 1080), /*extra_data=*/{},
      ::media::EncryptionScheme::kUnencrypted);
  video_config.set_level(42);
  return video_config;
}

// Returns a valid starboard audio config with values arbitrarily set. The
// values will match the values of GetChromiumAudioConfig.
StarboardAudioSampleInfo GetStarboardAudioConfig() {
  return StarboardAudioSampleInfo{
      .codec = kStarboardAudioCodecAac,
      .mime = R"-(audio/mp4; codecs="mp4a.40.5")-",
      .format_tag = 0,
      .number_of_channels = 2,
      .samples_per_second = 48000,
      .average_bytes_per_second = (32 / 8) * 48000 * 2,
      .block_alignment = 4,
      .bits_per_sample = 32,
      .audio_specific_config_size = 0,
      .audio_specific_config = nullptr,
  };
}

// Returns a valid starboard video config with values arbitrarily set. The
// values will match the values of GetChromiumVideoConfig.
StarboardVideoSampleInfo GetStarboardVideoConfig() {
  return StarboardVideoSampleInfo{
      .codec = kStarboardVideoCodecH264,
      .mime = R"-(video/mp4; codecs="avc1.64002A")-",
      .max_video_capabilities = "",
      .is_key_frame = false,
      .frame_width = 3840,
      .frame_height = 2160,
      .color_metadata =
          StarboardColorMetadata{
              // These 0 fields signify "unknown" to starboard.
              .bits_per_channel = 0,
              .chroma_subsampling_horizontal = 0,
              .chroma_subsampling_vertical = 0,
              .cb_subsampling_horizontal = 0,
              .cb_subsampling_vertical = 0,
              .chroma_siting_horizontal = 0,
              .chroma_siting_vertical = 0,
              // No HDR metadata, so everything is 0.
              .mastering_metadata = StarboardMediaMasteringMetadata{},
              .max_cll = 0,
              .max_fall = 0,
              .primaries = 1,  // BT.709
              .transfer = 1,   // BT.709
              .matrix = 1,     // BT.709
              .range = 1,      // broadcast range
              .custom_primary_matrix = {0},
          },
  };
}

// Some default configs.
const StarboardAudioSampleInfo kSbAudioConfig = GetStarboardAudioConfig();
const StarboardVideoSampleInfo kSbVideoConfig = GetStarboardVideoConfig();

// Some default audio/video data.
constexpr auto kAudioData = std::to_array<uint8_t>({1, 1, 1, 2, 2, 2});
constexpr auto kVideoData = std::to_array<uint8_t>({1, 2, 3, 4, 5, 6, 7});

// A test fixture is used to abstract away boilerplate (e.g. setting up the task
// environment, creating mocks).
class StarboardRendererTest : public ::testing::Test {
 protected:
  StarboardRendererTest() {
    mojo::core::Init();

    audio_stream_.set_audio_decoder_config(GetChromiumAudioConfig());
    video_stream_.set_video_decoder_config(GetChromiumVideoConfig());

    ON_CALL(starboard_for_drm_, CreateDrmSystem)
        .WillByDefault(Return(&drm_system_));
    StarboardDrmWrapper::SetSingletonForTesting(&starboard_for_drm_);

    ON_CALL(media_resource_, GetAllStreams)
        .WillByDefault(Return(
            std::vector<DemuxerStream*>({&audio_stream_, &video_stream_})));
    ON_CALL(media_resource_, GetFirstStream(DemuxerStream::Type::AUDIO))
        .WillByDefault(Return(&audio_stream_));
    ON_CALL(media_resource_, GetFirstStream(DemuxerStream::Type::VIDEO))
        .WillByDefault(Return(&video_stream_));

    ON_CALL(*starboard_,
            CreatePlayer(
                Pointee(MatchesPlayerCreationParam(StarboardPlayerCreationParam{
                    .drm_system = nullptr,
                    .audio_sample_info = kSbAudioConfig,
                    .video_sample_info = kSbVideoConfig,
                    .output_mode = StarboardPlayerOutputMode::
                        kStarboardPlayerOutputModePunchOut})),
                _))
        .WillByDefault(
            DoAll(SaveArg<1>(&sb_player_callbacks_), Return(&sb_player_)));
    ON_CALL(*starboard_, SeekTo(&sb_player_, _, _))
        .WillByDefault(SaveArg<2>(&current_seek_ticket_));

    // Since matchers compare data ptrs by address, it does not matter that all
    // of these buffers use the same audio data (the data is copied, so each
    // DecoderBuffer's data ptr will be different).
    scoped_refptr<::media::DecoderBuffer> audio_buffer_1 =
        ::media::DecoderBuffer::CopyFrom(kAudioData);
    audio_buffer_1->set_timestamp(base::Seconds(0));

    scoped_refptr<::media::DecoderBuffer> audio_buffer_2 =
        ::media::DecoderBuffer::CopyFrom(kAudioData);
    audio_buffer_2->set_timestamp(base::Seconds(1));

    scoped_refptr<::media::DecoderBuffer> audio_buffer_3 =
        ::media::DecoderBuffer::CopyFrom(kAudioData);
    audio_buffer_3->set_timestamp(base::Seconds(5));

    audio_buffers_ = {
        std::move(audio_buffer_1),
        std::move(audio_buffer_2),
        std::move(audio_buffer_3),
    };

    scoped_refptr<::media::DecoderBuffer> video_buffer_1 =
        ::media::DecoderBuffer::CopyFrom(kVideoData);
    video_buffer_1->set_timestamp(base::Seconds(0));

    scoped_refptr<::media::DecoderBuffer> video_buffer_2 =
        ::media::DecoderBuffer::CopyFrom(kVideoData);
    video_buffer_2->set_timestamp(base::Seconds(1));

    scoped_refptr<::media::DecoderBuffer> video_buffer_3 =
        ::media::DecoderBuffer::CopyFrom(kVideoData);
    video_buffer_3->set_timestamp(base::Seconds(5));

    scoped_refptr<::media::DecoderBuffer> video_buffer_4 =
        ::media::DecoderBuffer::CopyFrom(kVideoData);
    video_buffer_4->set_timestamp(base::Seconds(20));

    video_buffers_ = {
        std::move(video_buffer_1),
        std::move(video_buffer_2),
        std::move(video_buffer_3),
        std::move(video_buffer_4),
    };

    CHECK_EQ(audio_buffers_.size(), sb_audio_buffers_.size());
    for (size_t i = 0; i < audio_buffers_.size(); ++i) {
      const auto& audio_buffer = audio_buffers_[i];
      sb_audio_buffers_[i] = StarboardSampleInfo{
          .type = 0,
          .buffer = base::span(*audio_buffer).data(),
          .buffer_size = static_cast<int>(audio_buffer->size()),
          .timestamp = audio_buffer->timestamp().InMicroseconds(),
          .side_data = base::span<const StarboardSampleSideData>(),
          .audio_sample_info = kSbAudioConfig,
          .drm_info = nullptr,
      };
    }

    CHECK_EQ(video_buffers_.size(), sb_video_buffers_.size());
    for (size_t i = 0; i < video_buffers_.size(); ++i) {
      const auto& video_buffer = video_buffers_[i];
      sb_video_buffers_[i] = StarboardSampleInfo{
          .type = 1,
          .buffer = base::span(*video_buffer).data(),
          .buffer_size = static_cast<int>(video_buffer->size()),
          .timestamp = video_buffer->timestamp().InMicroseconds(),
          .side_data = base::span<const StarboardSampleSideData>(),
          .video_sample_info = kSbVideoConfig,
          .drm_info = nullptr,
      };
    }
  }

  ~StarboardRendererTest() override = default;

  // This should be destructed last.
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();
  NiceMock<::media::MockMediaResource> media_resource_;
  NiceMock<::media::MockDemuxerStream> audio_stream_ =
      NiceMock<::media::MockDemuxerStream>(DemuxerStream::Type::AUDIO);
  NiceMock<::media::MockDemuxerStream> video_stream_ =
      NiceMock<::media::MockDemuxerStream>(DemuxerStream::Type::VIDEO);
  NiceMock<::media::MockRendererClient> client_;
  NiceMock<chromecast::metrics::MockCastMetricsHelper> cast_metrics_helper_;
  MockFunction<void(::media::PipelineStatus)> pipeline_status_fn_;
  std::unique_ptr<NiceMock<MockStarboardApiWrapper>> starboard_ =
      std::make_unique<NiceMock<MockStarboardApiWrapper>>();

  // This will be set to the callbacks received by the mock Starboard. These
  // callbacks would normally be used by starboard, e.g. to request new buffers.
  // We can call these callbacks to simulate that behavior.
  const StarboardPlayerCallbackHandler* sb_player_callbacks_ = nullptr;

  // It is undefined behavior to set expectations on a mock after its mock
  // functions have been called. Thus, to be safe we use a separate mock
  // starboard for the StarboardDrmWrapper. All expectations are set before it
  // is passed to the StarboardDrmWrapper (in this fixture's ctor).
  NiceMock<MockStarboardApiWrapper> starboard_for_drm_;

  // Since SbPlayer is used as an opaque void* by cast, we can use any type
  // here. All that matters is the address.
  int sb_player_ = 1;
  // Same for SbDrmSystem.
  int drm_system_ = 2;
  // Captured by starboard when a seek is performed.
  int current_seek_ticket_ = -1;
  display::test::TestScreen test_screen_ =
      display::test::TestScreen(/*create_display=*/true,
                                /*register_screen=*/true);
  VideoGeometrySetterService geometry_setter_service_;

  // We use std::array here so we can have compile-time bounds checking (via
  // std::get()) and avoid for-loops when using these arrays in tests (to keep
  // EXPECT/ASSERT in top-level TEST bodies).
  //
  // Chromium buffers.
  std::array<scoped_refptr<::media::DecoderBuffer>, 3> audio_buffers_;
  std::array<scoped_refptr<::media::DecoderBuffer>, 4> video_buffers_;

  // Starboard buffers corresponding to the chromium buffers.
  std::array<StarboardSampleInfo, 3> sb_audio_buffers_;
  std::array<StarboardSampleInfo, 4> sb_video_buffers_;

  const base::UnguessableToken plane_id_ = base::UnguessableToken::Create();
};

TEST_F(StarboardRendererTest,
       ReadsFromDemuxerStreamsAndPushesBuffersToStarboard) {
  constexpr base::TimeDelta kSeekTime = base::Seconds(0);

  // Since we do not simulate geometry_setter_service_ receiving any geometry
  // changes, the player's bounds should end up getting set to the screen
  // resolution (full screen).
  EXPECT_CALL(
      *starboard_,
      SetPlayerBounds(
          &sb_player_, 0,
          static_cast<int>(display::test::TestScreen::kDefaultScreenBounds.x()),
          static_cast<int>(display::test::TestScreen::kDefaultScreenBounds.y()),
          static_cast<int>(
              display::test::TestScreen::kDefaultScreenBounds.width()),
          static_cast<int>(
              display::test::TestScreen::kDefaultScreenBounds.height())))
      .Times(1);
  EXPECT_CALL(client_, OnBufferingStateChange(
                           ::media::BufferingState::BUFFERING_HAVE_ENOUGH, _))
      .Times(AtLeast(1));
  EXPECT_CALL(*starboard_, SeekTo(&sb_player_, kSeekTime.InMicroseconds(), _))
      .WillOnce(SaveArg<2>(&current_seek_ticket_));

  // Audio buffer expectations.
  EXPECT_CALL(
      *starboard_,
      WriteSample(&sb_player_, StarboardMediaType::kStarboardMediaTypeAudio,
                  ElementsAre(MatchesStarboardSampleInfo(
                      std::get<0>(sb_audio_buffers_)))))
      .Times(1);
  EXPECT_CALL(
      *starboard_,
      WriteSample(&sb_player_, StarboardMediaType::kStarboardMediaTypeAudio,
                  ElementsAre(MatchesStarboardSampleInfo(
                      std::get<1>(sb_audio_buffers_)))))
      .Times(1);
  EXPECT_CALL(
      *starboard_,
      WriteSample(&sb_player_, StarboardMediaType::kStarboardMediaTypeAudio,
                  ElementsAre(MatchesStarboardSampleInfo(
                      std::get<2>(sb_audio_buffers_)))))
      .Times(1);
  EXPECT_CALL(*starboard_,
              WriteEndOfStream(&sb_player_,
                               StarboardMediaType::kStarboardMediaTypeAudio))
      .Times(1);

  // Video buffer expectations.
  EXPECT_CALL(
      *starboard_,
      WriteSample(&sb_player_, StarboardMediaType::kStarboardMediaTypeVideo,
                  ElementsAre(MatchesStarboardSampleInfo(
                      std::get<0>(sb_video_buffers_)))))
      .Times(1);
  EXPECT_CALL(
      *starboard_,
      WriteSample(&sb_player_, StarboardMediaType::kStarboardMediaTypeVideo,
                  ElementsAre(MatchesStarboardSampleInfo(
                      std::get<1>(sb_video_buffers_)))))
      .Times(1);
  EXPECT_CALL(
      *starboard_,
      WriteSample(&sb_player_, StarboardMediaType::kStarboardMediaTypeVideo,
                  ElementsAre(MatchesStarboardSampleInfo(
                      std::get<2>(sb_video_buffers_)))))
      .Times(1);
  EXPECT_CALL(
      *starboard_,
      WriteSample(&sb_player_, StarboardMediaType::kStarboardMediaTypeVideo,
                  ElementsAre(MatchesStarboardSampleInfo(
                      std::get<3>(sb_video_buffers_)))))
      .Times(1);
  EXPECT_CALL(*starboard_,
              WriteEndOfStream(&sb_player_,
                               StarboardMediaType::kStarboardMediaTypeVideo))
      .Times(1);

  // Provide the audio buffers from the audio DemuxerStream, ending with EOS.
  EXPECT_CALL(audio_stream_, OnRead)
      .WillOnce(
          RunOnceCallback<0>(DemuxerStream::Status::kOk,
                             std::vector<scoped_refptr<::media::DecoderBuffer>>(
                                 {std::get<0>(audio_buffers_)})))
      .WillOnce(
          RunOnceCallback<0>(DemuxerStream::Status::kOk,
                             std::vector<scoped_refptr<::media::DecoderBuffer>>(
                                 {std::get<1>(audio_buffers_)})))
      .WillOnce(
          RunOnceCallback<0>(DemuxerStream::Status::kOk,
                             std::vector<scoped_refptr<::media::DecoderBuffer>>(
                                 {std::get<2>(audio_buffers_)})))
      .WillOnce(
          RunOnceCallback<0>(DemuxerStream::Status::kOk,
                             std::vector<scoped_refptr<::media::DecoderBuffer>>(
                                 {::media::DecoderBuffer::CreateEOSBuffer()})));

  // Provide the video buffers from the video DemuxerStream, ending with EOS.
  EXPECT_CALL(video_stream_, OnRead)
      .WillOnce(
          RunOnceCallback<0>(DemuxerStream::Status::kOk,
                             std::vector<scoped_refptr<::media::DecoderBuffer>>(
                                 {std::get<0>(video_buffers_)})))
      .WillOnce(
          RunOnceCallback<0>(DemuxerStream::Status::kOk,
                             std::vector<scoped_refptr<::media::DecoderBuffer>>(
                                 {std::get<1>(video_buffers_)})))
      .WillOnce(
          RunOnceCallback<0>(DemuxerStream::Status::kOk,
                             std::vector<scoped_refptr<::media::DecoderBuffer>>(
                                 {std::get<2>(video_buffers_)})))
      .WillOnce(
          RunOnceCallback<0>(DemuxerStream::Status::kOk,
                             std::vector<scoped_refptr<::media::DecoderBuffer>>(
                                 {std::get<3>(video_buffers_)})))
      .WillOnce(
          RunOnceCallback<0>(DemuxerStream::Status::kOk,
                             std::vector<scoped_refptr<::media::DecoderBuffer>>(
                                 {::media::DecoderBuffer::CreateEOSBuffer()})));

  StarboardRenderer renderer(std::move(starboard_), task_runner_, plane_id_,
                             /*enable_buffering=*/true,
                             &geometry_setter_service_, &cast_metrics_helper_);

  EXPECT_CALL(pipeline_status_fn_,
              Call(HasStatusCode(::media::PipelineStatusCodes::PIPELINE_OK)))
      .Times(1);
  renderer.Initialize(
      &media_resource_, &client_,
      base::BindLambdaForTesting(pipeline_status_fn_.AsStdFunction()));
  RunPendingTasks();

  renderer.StartPlayingFrom(kSeekTime);

  ASSERT_THAT(sb_player_callbacks_, NotNull());
  ASSERT_THAT(sb_player_callbacks_->context, NotNull());
  ASSERT_THAT(sb_player_callbacks_->decoder_status_fn, NotNull());
  ASSERT_THAT(sb_player_callbacks_->player_status_fn, NotNull());

  // Simulate starboard requesting audio buffers. The +1 is to represent the
  // expected EOS after all buffers have been returned.
  for (size_t i = 0; i < audio_buffers_.size() + 1; ++i) {
    sb_player_callbacks_->decoder_status_fn(
        &sb_player_, sb_player_callbacks_->context,
        StarboardMediaType::kStarboardMediaTypeAudio,
        StarboardDecoderState::kStarboardDecoderStateNeedsData,
        current_seek_ticket_);
    RunPendingTasks();
  }

  // Simulate starboard requesting video buffers. The +1 is to represent the
  // expected EOS after all buffers have been returned.
  for (size_t i = 0; i < video_buffers_.size() + 1; ++i) {
    sb_player_callbacks_->decoder_status_fn(
        &sb_player_, sb_player_callbacks_->context,
        StarboardMediaType::kStarboardMediaTypeVideo,
        StarboardDecoderState::kStarboardDecoderStateNeedsData,
        current_seek_ticket_);
    RunPendingTasks();
  }

  // This should inform the RendererClient that we have enough data buffered.
  sb_player_callbacks_->player_status_fn(
      &sb_player_, sb_player_callbacks_->context,
      StarboardPlayerState::kStarboardPlayerStatePresenting,
      current_seek_ticket_);
}

TEST_F(StarboardRendererTest,
       FlushSeeksToCurrentMediaTimeAndSetsPlaybackRateToZero) {
  constexpr base::TimeDelta kSeekTime = base::Seconds(0);
  constexpr base::TimeDelta kMediaTime = base::Seconds(1);
  constexpr double kPlaybackRate = 1.1;

  EXPECT_CALL(*starboard_, SeekTo(&sb_player_, kSeekTime.InMicroseconds(), _))
      .WillOnce(SaveArg<2>(&current_seek_ticket_));
  EXPECT_CALL(*starboard_, SeekTo(&sb_player_, kMediaTime.InMicroseconds(), _))
      .WillOnce(SaveArg<2>(&current_seek_ticket_));

  // The playback rate should be set to zero twice:
  // * Once when we start playback
  // * Once when we flush
  EXPECT_CALL(*starboard_, SetPlaybackRate(&sb_player_, DoubleEq(0))).Times(2);
  // The playback rate should be set to 1.1 when we explicitly set the playback
  // rate.
  EXPECT_CALL(*starboard_,
              SetPlaybackRate(&sb_player_, DoubleEq(kPlaybackRate)))
      .Times(1);
  EXPECT_CALL(*starboard_, GetPlayerInfo(&sb_player_, NotNull()))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArg<1>([kMediaTime](StarboardPlayerInfo* player_info) {
            *player_info = {};
            player_info->current_media_timestamp_micros =
                kMediaTime.InMicroseconds();
          }));

  // Ignore unrelated metrics calls.
  EXPECT_CALL(cast_metrics_helper_, RecordApplicationEvent(_))
      .Times(AnyNumber());
  // Should be called at flush, but not at destruction.
  EXPECT_CALL(cast_metrics_helper_,
              RecordApplicationEvent("Cast.Platform.Ended"))
      .Times(1);
  // Should be called when setting the playback rate to a nonzero value.
  EXPECT_CALL(cast_metrics_helper_,
              RecordApplicationEvent("Cast.Platform.Playing"))
      .Times(AtLeast(1));

  StarboardRenderer renderer(std::move(starboard_), task_runner_, plane_id_,
                             /*enable_buffering=*/true,
                             &geometry_setter_service_, &cast_metrics_helper_);

  EXPECT_CALL(pipeline_status_fn_,
              Call(HasStatusCode(::media::PipelineStatusCodes::PIPELINE_OK)))
      .Times(1);
  renderer.Initialize(
      &media_resource_, &client_,
      base::BindLambdaForTesting(pipeline_status_fn_.AsStdFunction()));
  RunPendingTasks();

  renderer.StartPlayingFrom(kSeekTime);
  renderer.SetPlaybackRate(kPlaybackRate);

  MockFunction<void()> flush_cb;
  EXPECT_CALL(flush_cb, Call).Times(1);
  renderer.Flush(base::BindLambdaForTesting(flush_cb.AsStdFunction()));
  RunPendingTasks();
}

TEST_F(StarboardRendererTest, SetVolumeForwardsVolumeChangeToStarboard) {
  constexpr base::TimeDelta kSeekTime = base::Seconds(0);
  constexpr float kVolume = 0.77f;

  EXPECT_CALL(*starboard_, SetVolume(&sb_player_, DoubleEq(kVolume))).Times(1);

  StarboardRenderer renderer(std::move(starboard_), task_runner_, plane_id_,
                             /*enable_buffering=*/true,
                             &geometry_setter_service_, &cast_metrics_helper_);

  EXPECT_CALL(pipeline_status_fn_,
              Call(HasStatusCode(::media::PipelineStatusCodes::PIPELINE_OK)))
      .Times(1);
  renderer.Initialize(
      &media_resource_, &client_,
      base::BindLambdaForTesting(pipeline_status_fn_.AsStdFunction()));
  RunPendingTasks();

  renderer.StartPlayingFrom(kSeekTime);
  renderer.SetVolume(kVolume);
}

TEST_F(StarboardRendererTest, ForwardsGeometryChangesToStarboard) {
  const gfx::RectF geometry_1(0, 0, 1920, 1080);
  const gfx::RectF geometry_2(0, 0, 720, 1280);
  const gfx::OverlayTransform transform =
      gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;

  {
    InSequence s;
    EXPECT_CALL(
        *starboard_,
        SetPlayerBounds(&sb_player_, 0, static_cast<int>(geometry_1.x()),
                        static_cast<int>(geometry_1.y()),
                        static_cast<int>(geometry_1.width()),
                        static_cast<int>(geometry_1.height())))
        .Times(1);

    EXPECT_CALL(
        *starboard_,
        SetPlayerBounds(&sb_player_, 0, static_cast<int>(geometry_2.x()),
                        static_cast<int>(geometry_2.y()),
                        static_cast<int>(geometry_2.width()),
                        static_cast<int>(geometry_2.height())))
        .Times(1);
  }

  StarboardRenderer renderer(std::move(starboard_), task_runner_, plane_id_,
                             /*enable_buffering=*/true,
                             &geometry_setter_service_, &cast_metrics_helper_);
  RunPendingTasks();

  static_cast<mojom::VideoGeometrySetter*>(&geometry_setter_service_)
      ->SetVideoGeometry(geometry_1, transform, plane_id_);
  RunPendingTasks();

  EXPECT_CALL(pipeline_status_fn_,
              Call(HasStatusCode(::media::PipelineStatusCodes::PIPELINE_OK)))
      .Times(1);
  renderer.Initialize(
      &media_resource_, &client_,
      base::BindLambdaForTesting(pipeline_status_fn_.AsStdFunction()));
  RunPendingTasks();

  static_cast<mojom::VideoGeometrySetter*>(&geometry_setter_service_)
      ->SetVideoGeometry(geometry_2, transform, plane_id_);
  RunPendingTasks();
}

TEST_F(StarboardRendererTest, GetMediaTimeReadsCurrentMediaTimeFromStarboard) {
  constexpr base::TimeDelta kMediaTime = base::Seconds(1);

  EXPECT_CALL(*starboard_, GetPlayerInfo(&sb_player_, NotNull()))
      .Times(AtLeast(1))
      .WillRepeatedly(
          WithArg<1>([kMediaTime](StarboardPlayerInfo* player_info) {
            *player_info = {};
            player_info->current_media_timestamp_micros =
                kMediaTime.InMicroseconds();
          }));

  StarboardRenderer renderer(std::move(starboard_), task_runner_, plane_id_,
                             /*enable_buffering=*/true,
                             &geometry_setter_service_, &cast_metrics_helper_);

  EXPECT_CALL(pipeline_status_fn_,
              Call(HasStatusCode(::media::PipelineStatusCodes::PIPELINE_OK)))
      .Times(1);
  renderer.Initialize(
      &media_resource_, &client_,
      base::BindLambdaForTesting(pipeline_status_fn_.AsStdFunction()));
  RunPendingTasks();

  EXPECT_EQ(renderer.GetMediaTime(), kMediaTime);
}

TEST_F(StarboardRendererTest, SetPlaybackRateReportsMetric) {
  // Ignore unrelated metrics calls.
  EXPECT_CALL(cast_metrics_helper_, RecordApplicationEvent(_))
      .Times(AnyNumber());
  // Should be called at destruction.
  EXPECT_CALL(cast_metrics_helper_,
              RecordApplicationEvent("Cast.Platform.Ended"))
      .Times(1);
  // Should be called when setting the playback rate to a nonzero value.
  EXPECT_CALL(cast_metrics_helper_,
              RecordApplicationEvent("Cast.Platform.Playing"))
      .Times(1);
  // Should be called when setting the playback rate to 0.
  EXPECT_CALL(cast_metrics_helper_,
              RecordApplicationEvent("Cast.Platform.Pause"))
      .Times(1);

  StarboardRenderer renderer(std::move(starboard_), task_runner_, plane_id_,
                             /*enable_buffering=*/true,
                             &geometry_setter_service_, &cast_metrics_helper_);

  renderer.Initialize(
      &media_resource_, &client_,
      base::BindLambdaForTesting(pipeline_status_fn_.AsStdFunction()));
  RunPendingTasks();

  renderer.SetPlaybackRate(1.0);
  renderer.SetPlaybackRate(0.0);
}

}  // namespace
}  // namespace media
}  // namespace chromecast
