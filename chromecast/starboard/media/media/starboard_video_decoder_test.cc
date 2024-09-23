// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/starboard_video_decoder.h"

#include <optional>

#include "base/test/task_environment.h"
#include "chromecast/media/base/cast_decoder_buffer_impl.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/starboard/media/cdm/starboard_drm_key_tracker.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/encryption_scheme.h"
#include "media/base/subsample_entry.h"
#include "mock_starboard_api_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::testing::AllOf;
using ::testing::DoubleEq;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::MockFunction;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::StrEq;
using ::testing::WithArg;

// Converts between VideoCodec and StarboardVideoCodec.
StarboardVideoCodec ToStarboardVideoCodec(VideoCodec codec) {
  switch (codec) {
    case kCodecH264:
      return kStarboardVideoCodecH264;
    case kCodecVC1:
      return kStarboardVideoCodecVc1;
    case kCodecMPEG2:
      return kStarboardVideoCodecMpeg2;
    case kCodecTheora:
      return kStarboardVideoCodecTheora;
    case kCodecVP8:
      return kStarboardVideoCodecVp8;
    case kCodecVP9:
      return kStarboardVideoCodecVp9;
    case kCodecHEVC:
      return kStarboardVideoCodecH265;
    case kCodecAV1:
      return kStarboardVideoCodecAv1;
    default:
      return kStarboardVideoCodecNone;
  }
}

// Compares a StarboardSampleInfo (arg) to a VideoConfig (config) and a
// scoped_refptr<CastDecoderBuffer> (buffer).
MATCHER_P2(MatchesVideoConfigAndBuffer, config, buffer, "") {
  CHECK(buffer) << "Passed a null buffer to the matcher.";

  if (arg.type != kStarboardMediaTypeVideo) {
    *result_listener << "the StarboardSampleInfo's type is not video";
    return false;
  }

  // Check that the buffer's data matches.
  if (!ExplainMatchResult(
          ElementsAreArray(buffer->data(), buffer->data_size()),
          std::tuple<const uint8_t*, size_t>(
              static_cast<const uint8_t*>(arg.buffer), arg.buffer_size),
          result_listener)) {
    *result_listener << " the expected audio data";
    return false;
  }

  // Check the width/height.
  if (!ExplainMatchResult(
          AllOf(Field(&StarboardVideoSampleInfo::frame_width, config.width),
                Field(&StarboardVideoSampleInfo::frame_height, config.height)),
          arg.video_sample_info, result_listener)) {
    *result_listener << " the frame width/height";
    return false;
  }

  // Check the rest of the fields.
  return ExplainMatchResult(Eq(buffer->timestamp()), arg.timestamp,
                            result_listener) &&
         ExplainMatchResult(Eq(ToStarboardVideoCodec(config.codec)),
                            arg.video_sample_info.codec, result_listener) &&
         ExplainMatchResult(AllOf(Field(&StarboardColorMetadata::primaries,
                                        static_cast<int>(config.primaries)),
                                  Field(&StarboardColorMetadata::transfer,
                                        static_cast<int>(config.transfer)),
                                  Field(&StarboardColorMetadata::matrix,
                                        static_cast<int>(config.matrix)),
                                  Field(&StarboardColorMetadata::range,
                                        static_cast<int>(config.range))),
                            arg.video_sample_info.color_metadata,
                            result_listener);
}

// A mock delegate that can be passed to the decoder.
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

// A test fixture is used to manage the global mock state and to handle the
// lifetime of the SingleThreadTaskEnvironment.
class StarboardVideoDecoderTest : public ::testing::Test {
 protected:
  StarboardVideoDecoderTest()
      : starboard_(std::make_unique<MockStarboardApiWrapper>()) {
    // Ensure that tests begin with a clean slate regarding DRM keys.
    StarboardDrmKeyTracker::GetInstance().ClearStateForTesting();
  }

  ~StarboardVideoDecoderTest() override = default;

  // This should be destructed last.
  base::test::SingleThreadTaskEnvironment task_environment_;
  // This will be passed to the MediaPipelineBackendStarboard, and all calls to
  // Starboard will go through it. Thus, we can mock out those calls.
  std::unique_ptr<MockStarboardApiWrapper> starboard_;
  // Since SbPlayer is just an opaque blob to the MPB, we will simply use an int
  // to represent it.
  int fake_player_ = 1;
};

// Returns a simple VideoConfig.
VideoConfig GetBasicConfig() {
  VideoConfig config;

  config.codec = VideoCodec::kCodecH264;
  config.encryption_scheme = EncryptionScheme::kUnencrypted;
  config.width = 123;
  config.height = 456;

  return config;
}

TEST_F(StarboardVideoDecoderTest, PushesBufferToStarboard) {
  const VideoConfig config = GetBasicConfig();

  const std::vector<uint8_t> buffer_data = {1, 2, 3, 4, 5};
  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(), buffer_data.size());

  EXPECT_CALL(
      *starboard_,
      WriteSample(&fake_player_, kStarboardMediaTypeVideo,
                  Pointee(MatchesVideoConfigAndBuffer(config, buffer)), 1))
      .Times(1);

  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);
}

TEST_F(StarboardVideoDecoderTest, WritesEndOfStreamToStarboard) {
  EXPECT_CALL(*starboard_,
              WriteEndOfStream(&fake_player_, kStarboardMediaTypeVideo))
      .Times(1);

  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  const VideoConfig config = GetBasicConfig();

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  EXPECT_EQ(decoder.PushBuffer(CastDecoderBufferImpl::CreateEOSBuffer().get()),
            MediaPipelineBackend::BufferStatus::kBufferSuccess);
}

TEST_F(StarboardVideoDecoderTest, PopulatesDrmInfoInSamples) {
  // The length should be at most 16 bytes.
  constexpr char kKeyId[] = "key_id";
  // This must be 16 bytes.
  constexpr char kIv[] = "abcdefghijklmnop";
  // This must contain at least one subsample.
  const std::vector<::media::SubsampleEntry> subsamples = {
      ::media::SubsampleEntry(/*clear_bytes=*/1, /*cypher_bytes=*/2),
      ::media::SubsampleEntry(/*clear_bytes=*/3, /*cypher_bytes=*/4),
  };

  // If we do not add this key, buffers will not be pushed to starboard.
  StarboardDrmKeyTracker::GetInstance().AddKey(kKeyId, "session_id");

  VideoConfig config = GetBasicConfig();
  config.encryption_scheme = EncryptionScheme::kAesCbc;

  const ::media::EncryptionPattern encryption_pattern(5, 6);
  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCbcsConfig(kKeyId, kIv, subsamples,
                                               encryption_pattern);
  CHECK(decrypt_config);

  const std::vector<uint8_t> buffer_data = {1, 2, 3, 4, 5};
  scoped_refptr<::media::DecoderBuffer> decoder_buffer =
      ::media::DecoderBuffer::CopyFrom(buffer_data);
  CHECK(decoder_buffer);
  decoder_buffer->set_decrypt_config(std::move(decrypt_config));

  scoped_refptr<DecoderBufferAdapter> buffer =
      new DecoderBufferAdapter(decoder_buffer);

  StarboardDrmSampleInfo actual_drm_info = {};
  // The actual subsamples may be deleted after the call to
  // SbPlayerWriteSample2, so we need to store a copy.
  std::vector<StarboardDrmSubSampleMapping> actual_subsamples;
  EXPECT_CALL(
      *starboard_,
      WriteSample(&fake_player_, kStarboardMediaTypeVideo,
                  Pointee(AllOf(MatchesVideoConfigAndBuffer(config, buffer))),
                  1))
      .WillOnce(WithArg<2>([&actual_drm_info, &actual_subsamples](
                               StarboardSampleInfo* sample_infos) {
        // Since this is only called when the fourth argument is 1, that
        // means that sample_infos_count is 1.
        StarboardSampleInfo sample_info = sample_infos[0];
        if (!sample_info.drm_info) {
          return;
        }
        actual_drm_info = *sample_info.drm_info;
        const int subsample_count = actual_drm_info.subsample_count;
        if (subsample_count > 0) {
          actual_subsamples.assign(
              actual_drm_info.subsample_mapping,
              actual_drm_info.subsample_mapping + subsample_count);
        }
      }));

  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  EXPECT_EQ(actual_drm_info.encryption_scheme,
            kStarboardDrmEncryptionSchemeAesCbc);
  EXPECT_EQ(actual_drm_info.encryption_pattern.crypt_byte_block,
            encryption_pattern.crypt_byte_block());
  EXPECT_EQ(actual_drm_info.encryption_pattern.skip_byte_block,
            encryption_pattern.skip_byte_block());
  EXPECT_THAT(std::string(reinterpret_cast<const char*>(
                              actual_drm_info.initialization_vector),
                          actual_drm_info.initialization_vector_size),
              StrEq(kIv));
  EXPECT_THAT(
      std::string(reinterpret_cast<const char*>(actual_drm_info.identifier),
                  actual_drm_info.identifier_size),
      StrEq(kKeyId));
  EXPECT_THAT(
      actual_subsamples,
      ElementsAre(
          AllOf(Field(&StarboardDrmSubSampleMapping::clear_byte_count, 1),
                Field(&StarboardDrmSubSampleMapping::encrypted_byte_count, 2)),
          AllOf(
              Field(&StarboardDrmSubSampleMapping::clear_byte_count, 3),
              Field(&StarboardDrmSubSampleMapping::encrypted_byte_count, 4))));
}

TEST_F(StarboardVideoDecoderTest, DoesNotPushToStarboardIfDrmKeyIsUnavailable) {
  // The length should be at most 16 bytes.
  constexpr char kKeyId[] = "key_id";
  // This must be 16 bytes.
  constexpr char kIv[] = "abcdefghijklmnop";
  // This must contain at least one subsample.
  const std::vector<::media::SubsampleEntry> subsamples = {
      ::media::SubsampleEntry(/*clear_bytes=*/1, /*cypher_bytes=*/2),
      ::media::SubsampleEntry(/*clear_bytes=*/3, /*cypher_bytes=*/4),
  };

  // Since we do not add kKeyId to StarboardDrmKeyTracker, no buffer with this
  // key ID should be pushed to starboard.

  VideoConfig config = GetBasicConfig();
  config.encryption_scheme = EncryptionScheme::kAesCtr;

  const ::media::EncryptionPattern encryption_pattern(5, 6);
  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCbcsConfig(kKeyId, kIv, subsamples,
                                               encryption_pattern);
  CHECK(decrypt_config);

  const std::vector<uint8_t> buffer_data = {1, 2, 3, 4, 5};
  scoped_refptr<::media::DecoderBuffer> decoder_buffer =
      ::media::DecoderBuffer::CopyFrom(buffer_data);
  CHECK(decoder_buffer);
  decoder_buffer->set_decrypt_config(std::move(decrypt_config));

  scoped_refptr<DecoderBufferAdapter> buffer =
      new DecoderBufferAdapter(decoder_buffer);

  EXPECT_CALL(*starboard_, WriteSample).Times(0);

  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);
}

TEST_F(StarboardVideoDecoderTest,
       PushesBufferToStarboardAfterDrmKeyIsAvailable) {
  // The length should be at most 16 bytes.
  constexpr char kKeyId[] = "key_id";
  // This must be 16 bytes.
  constexpr char kIv[] = "abcdefghijklmnop";
  // This must contain at least one subsample.
  const std::vector<::media::SubsampleEntry> subsamples = {
      ::media::SubsampleEntry(/*clear_bytes=*/1, /*cypher_bytes=*/2),
      ::media::SubsampleEntry(/*clear_bytes=*/3, /*cypher_bytes=*/4),
  };

  VideoConfig config = GetBasicConfig();
  config.encryption_scheme = EncryptionScheme::kAesCbc;

  const ::media::EncryptionPattern encryption_pattern(5, 6);
  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCbcsConfig(kKeyId, kIv, subsamples,
                                               encryption_pattern);
  CHECK(decrypt_config);

  const std::vector<uint8_t> buffer_data = {1, 2, 3, 4, 5};
  scoped_refptr<::media::DecoderBuffer> decoder_buffer =
      ::media::DecoderBuffer::CopyFrom(buffer_data);
  CHECK(decoder_buffer);
  decoder_buffer->set_decrypt_config(std::move(decrypt_config));

  scoped_refptr<DecoderBufferAdapter> buffer =
      new DecoderBufferAdapter(decoder_buffer);

  StarboardDrmSampleInfo actual_drm_info = {};
  // The actual subsamples may be deleted after the call to
  // SbPlayerWriteSample2, so we need to store a copy.
  std::vector<StarboardDrmSubSampleMapping> actual_subsamples;
  EXPECT_CALL(
      *starboard_,
      WriteSample(&fake_player_, kStarboardMediaTypeVideo,
                  Pointee(AllOf(MatchesVideoConfigAndBuffer(config, buffer))),
                  1))
      .WillOnce(WithArg<2>([&actual_drm_info, &actual_subsamples](
                               StarboardSampleInfo* sample_infos) {
        // Since this is only called when the fourth argument is 1, that
        // means that sample_infos_count is 1.
        StarboardSampleInfo sample_info = sample_infos[0];
        if (!sample_info.drm_info) {
          return;
        }
        actual_drm_info = *sample_info.drm_info;
        const int subsample_count = actual_drm_info.subsample_count;
        if (subsample_count > 0) {
          actual_subsamples.assign(
              actual_drm_info.subsample_mapping,
              actual_drm_info.subsample_mapping + subsample_count);
        }
      }));

  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  // Now that the key is available, the buffer should be pushed to starboard.
  StarboardDrmKeyTracker::GetInstance().AddKey(kKeyId, "session_id");
  // The callback provided by the decoder will post a task; run until the
  // callback runs.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(actual_drm_info.encryption_scheme,
            kStarboardDrmEncryptionSchemeAesCbc);
  EXPECT_EQ(actual_drm_info.encryption_pattern.crypt_byte_block,
            encryption_pattern.crypt_byte_block());
  EXPECT_EQ(actual_drm_info.encryption_pattern.skip_byte_block,
            encryption_pattern.skip_byte_block());
  EXPECT_THAT(std::string(reinterpret_cast<const char*>(
                              actual_drm_info.initialization_vector),
                          actual_drm_info.initialization_vector_size),
              StrEq(kIv));
  EXPECT_THAT(
      std::string(reinterpret_cast<const char*>(actual_drm_info.identifier),
                  actual_drm_info.identifier_size),
      StrEq(kKeyId));
  EXPECT_THAT(
      actual_subsamples,
      ElementsAre(
          AllOf(Field(&StarboardDrmSubSampleMapping::clear_byte_count, 1),
                Field(&StarboardDrmSubSampleMapping::encrypted_byte_count, 2)),
          AllOf(
              Field(&StarboardDrmSubSampleMapping::clear_byte_count, 3),
              Field(&StarboardDrmSubSampleMapping::encrypted_byte_count, 4))));
}

TEST_F(StarboardVideoDecoderTest, ReturnsNulloptBeforeConfigSet) {
  StarboardVideoDecoder decoder(starboard_.get());

  EXPECT_EQ(decoder.GetVideoSampleInfo(), std::nullopt);
}

TEST_F(StarboardVideoDecoderTest,
       ReturnsPopulatedSampleInfoAfterConfigHasBeenSet) {
  StarboardVideoDecoder decoder(starboard_.get());

  VideoConfig config;
  config.codec = VideoCodec::kCodecVP8;
  config.encryption_scheme = EncryptionScheme::kUnencrypted;

  EXPECT_TRUE(decoder.SetConfig(config));
  EXPECT_THAT(decoder.GetVideoSampleInfo(),
              Optional(Field(&StarboardVideoSampleInfo::codec,
                             kStarboardVideoCodecVp8)));
}

TEST_F(StarboardVideoDecoderTest, PopulatesHdrInfo) {
  StarboardVideoDecoder decoder(starboard_.get());

  VideoConfig config;
  config.codec = VideoCodec::kCodecVP8;
  config.encryption_scheme = EncryptionScheme::kUnencrypted;
  config.have_hdr_metadata = true;
  config.hdr_metadata.max_content_light_level = 1;
  config.hdr_metadata.max_frame_average_light_level = 2;

  auto& color_volume_metadata = config.hdr_metadata.color_volume_metadata;
  color_volume_metadata.primary_r_chromaticity_x = 1;
  color_volume_metadata.primary_r_chromaticity_y = 2;
  color_volume_metadata.primary_g_chromaticity_x = 3;
  color_volume_metadata.primary_g_chromaticity_y = 4;
  color_volume_metadata.primary_b_chromaticity_x = 5;
  color_volume_metadata.primary_b_chromaticity_y = 6;
  color_volume_metadata.white_point_chromaticity_x = 7;
  color_volume_metadata.white_point_chromaticity_y = 8;
  color_volume_metadata.luminance_min = 9;
  color_volume_metadata.luminance_max = 10;

  EXPECT_TRUE(decoder.SetConfig(config));
  const std::optional<StarboardVideoSampleInfo> sample_info =
      decoder.GetVideoSampleInfo();

  ASSERT_TRUE(sample_info.has_value());
  EXPECT_EQ(sample_info->codec, kStarboardVideoCodecVp8);
  EXPECT_EQ(sample_info->color_metadata.max_cll, 1UL);
  EXPECT_EQ(sample_info->color_metadata.max_fall, 2UL);
  EXPECT_EQ(
      sample_info->color_metadata.mastering_metadata.primary_r_chromaticity_x,
      1);
  EXPECT_EQ(
      sample_info->color_metadata.mastering_metadata.primary_r_chromaticity_y,
      2);
  EXPECT_EQ(
      sample_info->color_metadata.mastering_metadata.primary_g_chromaticity_x,
      3);
  EXPECT_EQ(
      sample_info->color_metadata.mastering_metadata.primary_g_chromaticity_y,
      4);
  EXPECT_EQ(
      sample_info->color_metadata.mastering_metadata.primary_b_chromaticity_x,
      5);
  EXPECT_EQ(
      sample_info->color_metadata.mastering_metadata.primary_b_chromaticity_y,
      6);
  EXPECT_EQ(
      sample_info->color_metadata.mastering_metadata.white_point_chromaticity_x,
      7);
  EXPECT_EQ(
      sample_info->color_metadata.mastering_metadata.white_point_chromaticity_y,
      8);
  EXPECT_EQ(sample_info->color_metadata.mastering_metadata.luminance_min, 9);
  EXPECT_EQ(sample_info->color_metadata.mastering_metadata.luminance_max, 10);
}

TEST_F(StarboardVideoDecoderTest,
       HandlesMultiplePushBuffersBeforeInitialization) {
  const std::vector<uint8_t> buffer_data_1 = {1, 2, 3, 4, 5};
  scoped_refptr<CastDecoderBufferImpl> buffer_1(
      new CastDecoderBufferImpl(buffer_data_1.size()));
  memcpy(buffer_1->writable_data(), buffer_data_1.data(), buffer_data_1.size());

  const std::vector<uint8_t> buffer_data_2 = {6, 7, 8, 9, 10};
  scoped_refptr<CastDecoderBufferImpl> buffer_2(
      new CastDecoderBufferImpl(buffer_data_2.size()));
  memcpy(buffer_2->writable_data(), buffer_data_2.data(), buffer_data_2.size());

  const VideoConfig config = GetBasicConfig();

  // Only the last buffer -- buffer_2 -- should be sent to starboard once the
  // decoder is initialized.
  EXPECT_CALL(
      *starboard_,
      WriteSample(&fake_player_, kStarboardMediaTypeVideo,
                  Pointee(MatchesVideoConfigAndBuffer(config, buffer_1)), 1))
      .Times(0);
  EXPECT_CALL(
      *starboard_,
      WriteSample(&fake_player_, kStarboardMediaTypeVideo,
                  Pointee(MatchesVideoConfigAndBuffer(config, buffer_2)), 1))
      .Times(1);

  StarboardVideoDecoder decoder(starboard_.get());
  decoder.SetConfig(config);
  EXPECT_EQ(decoder.PushBuffer(buffer_1.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);
  EXPECT_EQ(decoder.PushBuffer(buffer_2.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  MockDelegate delegate;
  decoder.SetDelegate(&delegate);

  // At this point, the pending buffer (buffer_2) should be pushed.
  decoder.Initialize(&fake_player_);
}

TEST_F(StarboardVideoDecoderTest, DoesNotCallDelegateEoSWhenPushingEoSBuffer) {
  const VideoConfig config = GetBasicConfig();

  StarboardVideoDecoder decoder(starboard_.get());

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  MockDelegate delegate;
  // This should not be called, since we never call
  // decoder.OnSbPlayerEndOfStream() in this test.
  EXPECT_CALL(delegate, OnEndOfStream).Times(0);
  decoder.SetDelegate(&delegate);

  EXPECT_EQ(decoder.PushBuffer(CastDecoderBufferImpl::CreateEOSBuffer().get()),
            MediaPipelineBackend::BufferStatus::kBufferSuccess);
}

TEST_F(StarboardVideoDecoderTest, CallsDelegateEoSWhenSbPlayerStreamEnds) {
  const VideoConfig config = GetBasicConfig();

  StarboardVideoDecoder decoder(starboard_.get());

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  MockDelegate delegate;
  EXPECT_CALL(delegate, OnEndOfStream).Times(1);
  decoder.SetDelegate(&delegate);
  decoder.OnSbPlayerEndOfStream();
}

TEST_F(StarboardVideoDecoderTest, ReportsStatistics) {
  constexpr int kTotalVideoFrames = 3;
  constexpr int kDroppedFrames = 1;

  EXPECT_CALL(*starboard_, GetPlayerInfo(&fake_player_, NotNull()))
      .WillOnce(WithArg<1>([](StarboardPlayerInfo* player_info) {
        *player_info = {};
        player_info->total_video_frames = kTotalVideoFrames;
        player_info->dropped_video_frames = kDroppedFrames;
      }));

  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  const VideoConfig config = GetBasicConfig();
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  const std::vector<uint8_t> buffer_data = {1, 2, 3, 4, 5};
  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(buffer_data.size()));
  memcpy(buffer->writable_data(), buffer_data.data(), buffer_data.size());

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  MediaPipelineBackend::VideoDecoder::Statistics stats = {};
  decoder.GetStatistics(&stats);
  EXPECT_EQ(stats.decoded_bytes, buffer_data.size());
  EXPECT_EQ(stats.decoded_frames, static_cast<uint64_t>(kTotalVideoFrames));
  EXPECT_EQ(stats.dropped_frames, static_cast<uint64_t>(kDroppedFrames));
}

TEST_F(StarboardVideoDecoderTest,
       DoesNotInformDelegateWhenResolutionChangesBeforePushBuffer) {
  constexpr int kNewWidth = 10;
  constexpr int kNewHeight = 20;

  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  EXPECT_CALL(delegate, OnVideoResolutionChanged).Times(0);

  VideoConfig config = GetBasicConfig();
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Update the config. This should NOT notify the delegate, because the
  // pipeline may not be in the kPlaying state.
  config.width = kNewWidth;
  config.height = kNewHeight;
  decoder.SetConfig(config);
}

TEST_F(StarboardVideoDecoderTest,
       InformsDelegateWhenResolutionChangesAtNextPushBuffer) {
  constexpr int kNewWidth = 10;
  constexpr int kNewHeight = 20;

  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  EXPECT_CALL(delegate, OnVideoResolutionChanged(AllOf(
                            Field(&chromecast::Size::width, Eq(kNewWidth)),
                            Field(&chromecast::Size::height, Eq(kNewHeight)))))
      .Times(1);

  VideoConfig config = GetBasicConfig();
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Update the config with the new resolution.
  config.width = kNewWidth;
  config.height = kNewHeight;
  decoder.SetConfig(config);

  scoped_refptr<CastDecoderBufferImpl> buffer(new CastDecoderBufferImpl(10));
  // Pushing the buffer should trigger the call to OnVideoResolutionChanged.
  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);
}

TEST_F(StarboardVideoDecoderTest, PopulatesMimeTypeForHEVCDolbyVision) {
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecDolbyVisionHEVC;
  config.profile = kDolbyVisionProfile5;
  config.codec_profile_level = 6;

  StarboardVideoDecoder decoder(starboard_.get());
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);

  const std::optional<StarboardVideoSampleInfo>& video_info =
      decoder.GetVideoSampleInfo();

  ASSERT_NE(video_info, std::nullopt);
  EXPECT_THAT(video_info->mime, StrEq(R"-(video/mp4; codecs="dvhe.05.06")-"));
}

}  // namespace
}  // namespace media
}  // namespace chromecast
