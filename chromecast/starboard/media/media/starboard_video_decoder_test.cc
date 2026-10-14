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
using ::testing::AnyNumber;
using ::testing::DoubleEq;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::Mock;
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
    *result_listener << " the expected video data";
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
  auto buffer = base::MakeRefCounted<DecoderBufferAdapter>(
      ::media::DecoderBuffer::CopyFrom(buffer_data));

  EXPECT_CALL(
      *starboard_,
      WriteSample(&fake_player_, kStarboardMediaTypeVideo,
                  ElementsAre(MatchesVideoConfigAndBuffer(config, buffer))))
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

TEST_F(StarboardVideoDecoderTest,
       EndOfStreamAfterStopDoesNotPushToNullSbPlayer) {
  // Regression test for crbug.com/375652489.
  EXPECT_CALL(*starboard_, WriteEndOfStream(nullptr, kStarboardMediaTypeVideo))
      .Times(0);
  EXPECT_CALL(*starboard_,
              WriteEndOfStream(&fake_player_, kStarboardMediaTypeVideo))
      .Times(1);

  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  const VideoConfig config = GetBasicConfig();

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);
  decoder.Stop();

  EXPECT_EQ(decoder.PushBuffer(CastDecoderBufferImpl::CreateEOSBuffer().get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  // This should trigger the WriteEndOfStream call.
  decoder.Initialize(&fake_player_);
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

  const auto encryption_pattern = ::media::EncryptionPattern::Create(5, 6);
  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCbcsConfig(kKeyId, kIv, subsamples,
                                               encryption_pattern);
  CHECK(decrypt_config);

  // The subsamples above account for 1+2+3+4 = 10 bytes, and
  // DecoderBufferAdapter replaces any buffer whose size does not match its
  // subsamples with an end-of-stream buffer. The data must therefore be
  // exactly 10 bytes for this test to exercise the DRM path at all.
  const std::vector<uint8_t> buffer_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
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
      WriteSample(
          &fake_player_, kStarboardMediaTypeVideo,
          ElementsAre(AllOf(MatchesVideoConfigAndBuffer(config, buffer)))))
      .WillOnce(
          WithArg<2>([&actual_drm_info, &actual_subsamples](
                         base::span<const StarboardSampleInfo> sample_infos) {
            // The "ElementsAre" matcher above ensures that there is exactly one
            // element in sample_infos.
            CHECK_EQ(sample_infos.size(), 1UL);
            StarboardSampleInfo sample_info = sample_infos[0];
            if (!sample_info.drm_info) {
              return;
            }
            actual_drm_info = *sample_info.drm_info;
            actual_subsamples.assign(actual_drm_info.subsample_mapping.begin(),
                                     actual_drm_info.subsample_mapping.end());
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
            encryption_pattern->crypt_byte_block());
  EXPECT_EQ(actual_drm_info.encryption_pattern.skip_byte_block,
            encryption_pattern->skip_byte_block());
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

  const auto encryption_pattern = ::media::EncryptionPattern::Create(5, 6);
  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCbcsConfig(kKeyId, kIv, subsamples,
                                               encryption_pattern);
  CHECK(decrypt_config);

  // The subsamples above account for 1+2+3+4 = 10 bytes, and
  // DecoderBufferAdapter replaces any buffer whose size does not match its
  // subsamples with an end-of-stream buffer. The data must therefore be
  // exactly 10 bytes for this test to exercise the DRM path at all.
  const std::vector<uint8_t> buffer_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
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

  const auto encryption_pattern = ::media::EncryptionPattern::Create(5, 6);
  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCbcsConfig(kKeyId, kIv, subsamples,
                                               encryption_pattern);
  CHECK(decrypt_config);

  // The subsamples above account for 1+2+3+4 = 10 bytes, and
  // DecoderBufferAdapter replaces any buffer whose size does not match its
  // subsamples with an end-of-stream buffer. The data must therefore be
  // exactly 10 bytes for this test to exercise the DRM path at all.
  const std::vector<uint8_t> buffer_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
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
      WriteSample(
          &fake_player_, kStarboardMediaTypeVideo,
          ElementsAre(AllOf(MatchesVideoConfigAndBuffer(config, buffer)))))
      .WillOnce(
          WithArg<2>([&actual_drm_info, &actual_subsamples](
                         base::span<const StarboardSampleInfo> sample_infos) {
            // The "ElementsAre" matcher above ensures that there is exactly one
            // element in sample_infos.
            CHECK_EQ(sample_infos.size(), 1UL);
            StarboardSampleInfo sample_info = sample_infos[0];
            if (!sample_info.drm_info) {
              return;
            }
            actual_drm_info = *sample_info.drm_info;
            actual_subsamples.assign(actual_drm_info.subsample_mapping.begin(),
                                     actual_drm_info.subsample_mapping.end());
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
            encryption_pattern->crypt_byte_block());
  EXPECT_EQ(actual_drm_info.encryption_pattern.skip_byte_block,
            encryption_pattern->skip_byte_block());
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
  auto buffer_1 = base::MakeRefCounted<DecoderBufferAdapter>(
      ::media::DecoderBuffer::CopyFrom(buffer_data_1));

  const std::vector<uint8_t> buffer_data_2 = {6, 7, 8, 9, 10};
  auto buffer_2 = base::MakeRefCounted<DecoderBufferAdapter>(
      ::media::DecoderBuffer::CopyFrom(buffer_data_2));

  const VideoConfig config = GetBasicConfig();

  // Only the last buffer -- buffer_2 -- should be sent to starboard once the
  // decoder is initialized.
  EXPECT_CALL(
      *starboard_,
      WriteSample(&fake_player_, kStarboardMediaTypeVideo,
                  ElementsAre(MatchesVideoConfigAndBuffer(config, buffer_1))))
      .Times(0);
  EXPECT_CALL(
      *starboard_,
      WriteSample(&fake_player_, kStarboardMediaTypeVideo,
                  ElementsAre(MatchesVideoConfigAndBuffer(config, buffer_2))))
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
  auto buffer = base::MakeRefCounted<DecoderBufferAdapter>(
      ::media::DecoderBuffer::CopyFrom(buffer_data));

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

TEST_F(StarboardVideoDecoderTest, InBandH264SpsUpdatesResolution) {
  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  // Initial config is 1280x720.
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecH264;
  config.width = 1280;
  config.height = 720;
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // SPS NAL unit for 320x240 video.
  const uint8_t kSpsBuffer[] = {0x00, 0x00, 0x00, 0x01, 0x27, 0x4d, 0x40, 0x0d,
                                0xa9, 0x18, 0x28, 0x3e, 0x60, 0x0d, 0x41, 0x80,
                                0x41, 0xad, 0xb0, 0xad, 0x7b, 0xdf, 0x01, 0x00};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kSpsBuffer)));
  // SAFETY: In unit test, buffer has sufficient size for constant test array.
  UNSAFE_BUFFERS(base::span(buffer->writable_data(), sizeof(kSpsBuffer))
                     .copy_from(base::span(kSpsBuffer)));

  // Pushing this buffer containing in-band SPS must notify delegate of (320,
  // 240).
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(320)),
                                  Field(&chromecast::Size::height, Eq(240)))))
      .Times(1);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 320);
  EXPECT_EQ(video_info->frame_height, 240);
}

TEST_F(StarboardVideoDecoderTest, InBandVp9UpdatesResolution) {
  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  // Initial config is 1280x720.
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP9;
  config.width = 1280;
  config.height = 720;
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // VP9 Keyframe buffer for 320x240 video.
  const uint8_t kVp9KeyframeBuffer[] = {
      0x82, 0x49, 0x83, 0x42, 0x00, 0x13, 0xf0, 0x0e, 0xf6, 0x12, 0x38, 0x24,
      0x1c, 0x18, 0x82, 0x00, 0x07, 0x80, 0x7f, 0xd1, 0xfa, 0x3e, 0x8f, 0xe7,
      0x71, 0xfb, 0xfe, 0xfc, 0x2f, 0xf3, 0xbf, 0xce, 0xff, 0x67, 0xf3, 0x7e,
      0x7f, 0x8d, 0xf0, 0xfd, 0x1f, 0xac, 0xf7, 0x82, 0xfe, 0x5f, 0xc9, 0xfe,
      0x67, 0xc8, 0xff, 0xb8, 0xf7, 0x9b, 0xf7, 0xd9, 0xff, 0x6f, 0xe7, 0xff,
      0xc6, 0xfc, 0x2f, 0xd1, 0xf6, 0x77, 0xfc, 0x27, 0xc6, 0xff, 0xa7, 0xfb,
      0x77, 0xff, 0x09, 0xf0, 0xbf, 0xf0, 0x3f, 0xe7, 0xbe, 0xee, 0x3e, 0xce,
      0xff, 0x7b, 0xdb, 0x7f, 0x27, 0xc4, 0xf4, 0xbe, 0x9f, 0xde, 0x1b, 0xd1,
      0x75, 0xc7, 0xfb, 0xd1, 0xfb, 0xc6, 0x7b, 0xc3, 0x7a, 0x42, 0x72, 0xdc,
      0xec, 0x7c, 0x22, 0xc1, 0xee, 0xce, 0xdf, 0xec, 0x62, 0x07, 0x58, 0x2a,
      0x13, 0x45, 0x53, 0xda, 0x6f, 0x7e, 0x07, 0xc1, 0xb7, 0x60, 0x13, 0xba,
      0xf8, 0xa4, 0x01, 0x53, 0x20, 0x00, 0x7d, 0xd4, 0x37, 0x1d, 0xff, 0x6d,
      0x05, 0xab, 0x96, 0x6b, 0x3d, 0xcd, 0x78, 0x4b, 0x22, 0x83, 0x15, 0x17,
      0x84, 0x11, 0xc5, 0xac, 0x92, 0x81, 0xf1, 0x17, 0x72, 0xd7, 0xe7, 0xc6,
      0x04,
  };

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kVp9KeyframeBuffer)));
  // SAFETY: In unit test, buffer has sufficient size for constant test array.
  UNSAFE_BUFFERS(base::span(buffer->writable_data(), sizeof(kVp9KeyframeBuffer))
                     .copy_from(base::span(kVp9KeyframeBuffer)));

  // Pushing this buffer containing in-band VP9 keyframe header must notify
  // delegate of (320, 240).
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(320)),
                                  Field(&chromecast::Size::height, Eq(240)))))
      .Times(1);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 320);
  EXPECT_EQ(video_info->frame_height, 240);
}

TEST_F(StarboardVideoDecoderTest, InBandVp8UpdatesResolution) {
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP8;
  config.width = 1280;
  config.height = 720;

  MockDelegate delegate;
  StarboardVideoDecoder decoder(starboard_.get());
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Construct a minimal VP8 keyframe buffer (10 bytes header for 320x240):
  // Byte 0: frame tag (bit 0 = 0 for keyframe, bit 4 = 1 for show_frame) ->
  // 0x10 Byte 1: version 0 -> 0x00 Byte 2: first partition size -> 0x00 Bytes
  // 3-5: VP8 start code 0x9d, 0x01, 0x2a Bytes 6-7: width 320 (0x0140) -> 0x40,
  // 0x01 Bytes 8-9: height 240 (0x00f0) -> 0xf0, 0x00
  constexpr uint8_t kVp8KeyframeBuffer[] = {
      0x10, 0x00, 0x00, 0x9d, 0x01, 0x2a, 0x40, 0x01, 0xf0, 0x00,
  };

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kVp8KeyframeBuffer)));
  // SAFETY: In unit test, buffer has sufficient size for constant test array.
  UNSAFE_BUFFERS(base::span(buffer->writable_data(), sizeof(kVp8KeyframeBuffer))
                     .copy_from(base::span(kVp8KeyframeBuffer)));

  // Pushing this buffer containing in-band VP8 keyframe header must notify
  // delegate of (320, 240).
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(320)),
                                  Field(&chromecast::Size::height, Eq(240)))))
      .Times(1);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 320);
  EXPECT_EQ(video_info->frame_height, 240);
}

TEST_F(StarboardVideoDecoderTest, InBandResolutionChangeIsIdempotent) {
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP8;
  config.width = 1280;
  config.height = 720;

  MockDelegate delegate;
  StarboardVideoDecoder decoder(starboard_.get());
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  constexpr uint8_t kVp8KeyframeBuffer[] = {
      0x10, 0x00, 0x00, 0x9d, 0x01, 0x2a, 0x40, 0x01, 0xf0, 0x00,
  };

  scoped_refptr<CastDecoderBufferImpl> buffer1(
      new CastDecoderBufferImpl(sizeof(kVp8KeyframeBuffer)));
  UNSAFE_BUFFERS(
      base::span(buffer1->writable_data(), sizeof(kVp8KeyframeBuffer))
          .copy_from(base::span(kVp8KeyframeBuffer)));

  scoped_refptr<CastDecoderBufferImpl> buffer2(
      new CastDecoderBufferImpl(sizeof(kVp8KeyframeBuffer)));
  UNSAFE_BUFFERS(
      base::span(buffer2->writable_data(), sizeof(kVp8KeyframeBuffer))
          .copy_from(base::span(kVp8KeyframeBuffer)));

  // Delegate must only be notified ONCE when resolution does not change.
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(320)),
                                  Field(&chromecast::Size::height, Eq(240)))))
      .Times(1);

  EXPECT_EQ(decoder.PushBuffer(buffer1.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);
  EXPECT_EQ(decoder.PushBuffer(buffer2.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);
}

TEST_F(StarboardVideoDecoderTest, InBandEncryptedBufferDoesNotParseResolution) {
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecH264;
  config.width = 1280;
  config.height = 720;
  config.encryption_scheme = EncryptionScheme::kAesCtr;

  MockDelegate delegate;
  StarboardVideoDecoder decoder(starboard_.get());
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Push an initial buffer to flush the initial SetConfig() resolution
  // notification.
  scoped_refptr<CastDecoderBufferImpl> init_buffer(
      new CastDecoderBufferImpl(10));
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(1280)),
                                  Field(&chromecast::Size::height, Eq(720)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(init_buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const uint8_t kSpsBuffer[] = {0x00, 0x00, 0x00, 0x01, 0x27, 0x4d, 0x40, 0x0d,
                                0xa9, 0x18, 0x28, 0x3e, 0x60, 0x0d, 0x41, 0x80,
                                0x41, 0xad, 0xb0, 0xad, 0x7b, 0xdf, 0x01, 0x00};

  const auto encryption_pattern = ::media::EncryptionPattern::Create(0, 0);
  const std::vector<::media::SubsampleEntry> subsamples = {
      ::media::SubsampleEntry(/*clear_bytes=*/0,
                              /*cypher_bytes=*/sizeof(kSpsBuffer)),
  };
  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCencConfig("key_id_16_bytes_",
                                               "iv_is_16_bytes__", subsamples);

  scoped_refptr<::media::DecoderBuffer> decoder_buffer =
      ::media::DecoderBuffer::CopyFrom(kSpsBuffer);
  decoder_buffer->set_decrypt_config(std::move(decrypt_config));

  scoped_refptr<DecoderBufferAdapter> buffer =
      new DecoderBufferAdapter(decoder_buffer);

  // Encrypted buffer containing SPS should NOT trigger resolution changes.
  EXPECT_CALL(delegate, OnVideoResolutionChanged).Times(0);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 1280);
  EXPECT_EQ(video_info->frame_height, 720);
}

TEST_F(StarboardVideoDecoderTest, InBandTruncatedVp8BufferDoesNotCrash) {
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP8;
  config.width = 1280;
  config.height = 720;

  MockDelegate delegate;
  StarboardVideoDecoder decoder(starboard_.get());
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Truncated buffer (less than 10 bytes).
  constexpr uint8_t kTruncatedBuffer[] = {0x10, 0x00, 0x00, 0x9d, 0x01};

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kTruncatedBuffer)));
  UNSAFE_BUFFERS(base::span(buffer->writable_data(), sizeof(kTruncatedBuffer))
                     .copy_from(base::span(kTruncatedBuffer)));

  // Initial SetConfig resolution is reported on the first buffer, but truncated
  // VP8 header does not trigger any new resolution change.
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(1280)),
                                  Field(&chromecast::Size::height, Eq(720)))))
      .Times(1);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  // Resolution should remain at initial config.
  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 1280);
  EXPECT_EQ(video_info->frame_height, 720);
}

TEST_F(StarboardVideoDecoderTest, InBandVp8InterframeDoesNotChangeResolution) {
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP8;
  config.width = 1280;
  config.height = 720;

  MockDelegate delegate;
  StarboardVideoDecoder decoder(starboard_.get());
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Push an initial buffer to flush the initial SetConfig() resolution
  // notification.
  scoped_refptr<CastDecoderBufferImpl> init_buffer(
      new CastDecoderBufferImpl(10));
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(1280)),
                                  Field(&chromecast::Size::height, Eq(720)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(init_buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  // VP8 interframe buffer: bit 0 of byte 0 is 1.
  constexpr uint8_t kVp8InterframeBuffer[] = {
      0x11, 0x00, 0x00, 0x9d, 0x01, 0x2a, 0x40, 0x01, 0xf0, 0x00,
  };
  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kVp8InterframeBuffer)));
  UNSAFE_BUFFERS(
      base::span(buffer->writable_data(), sizeof(kVp8InterframeBuffer))
          .copy_from(base::span(kVp8InterframeBuffer)));

  // Interframe should NOT trigger any resolution change.
  EXPECT_CALL(delegate, OnVideoResolutionChanged).Times(0);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 1280);
  EXPECT_EQ(video_info->frame_height, 720);
}

TEST_F(StarboardVideoDecoderTest, InBandH264NonSpsDoesNotChangeResolution) {
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecH264;
  config.width = 1280;
  config.height = 720;

  MockDelegate delegate;
  StarboardVideoDecoder decoder(starboard_.get());
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Push an initial buffer to flush the initial SetConfig() resolution
  // notification.
  scoped_refptr<CastDecoderBufferImpl> init_buffer(
      new CastDecoderBufferImpl(10));
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(1280)),
                                  Field(&chromecast::Size::height, Eq(720)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(init_buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  // H.264 non-IDR slice NAL unit (nal_unit_type = 1).
  constexpr uint8_t kNonSpsNalu[] = {0x00, 0x00, 0x00, 0x01, 0x01, 0xff, 0xff};
  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kNonSpsNalu)));
  UNSAFE_BUFFERS(base::span(buffer->writable_data(), sizeof(kNonSpsNalu))
                     .copy_from(base::span(kNonSpsNalu)));

  // Non-SPS NALU should NOT trigger any resolution change.
  EXPECT_CALL(delegate, OnVideoResolutionChanged).Times(0);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 1280);
  EXPECT_EQ(video_info->frame_height, 720);
}

TEST_F(StarboardVideoDecoderTest, InBandTruncatedH264BufferDoesNotCrash) {
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecH264;
  config.width = 1280;
  config.height = 720;

  MockDelegate delegate;
  StarboardVideoDecoder decoder(starboard_.get());
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  scoped_refptr<CastDecoderBufferImpl> init_buffer(
      new CastDecoderBufferImpl(10));
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(1280)),
                                  Field(&chromecast::Size::height, Eq(720)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(init_buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  // Truncated H.264 start code (only 2 bytes).
  constexpr uint8_t kTruncatedH264[] = {0x00, 0x00};
  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kTruncatedH264)));
  UNSAFE_BUFFERS(base::span(buffer->writable_data(), sizeof(kTruncatedH264))
                     .copy_from(base::span(kTruncatedH264)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged).Times(0);
  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 1280);
  EXPECT_EQ(video_info->frame_height, 720);
}

TEST_F(StarboardVideoDecoderTest, InBandTruncatedVp9BufferDoesNotCrash) {
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP9;
  config.width = 1280;
  config.height = 720;

  MockDelegate delegate;
  StarboardVideoDecoder decoder(starboard_.get());
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  scoped_refptr<CastDecoderBufferImpl> init_buffer(
      new CastDecoderBufferImpl(10));
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(1280)),
                                  Field(&chromecast::Size::height, Eq(720)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(init_buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  // Truncated VP9 frame header (only 1 byte).
  constexpr uint8_t kTruncatedVp9[] = {0x82};
  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kTruncatedVp9)));
  UNSAFE_BUFFERS(base::span(buffer->writable_data(), sizeof(kTruncatedVp9))
                     .copy_from(base::span(kTruncatedVp9)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged).Times(0);
  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 1280);
  EXPECT_EQ(video_info->frame_height, 720);
}

TEST_F(StarboardVideoDecoderTest, InBandVp8ScaleCodeAppliesMultiplier) {
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP8;
  config.width = 1280;
  config.height = 720;

  MockDelegate delegate;
  StarboardVideoDecoder decoder(starboard_.get());
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Width 320 (0x0140) with horiz_scale = 1 (5/4) -> 320 * 5 / 4 = 400.
  // Byte 6: 0x40, Byte 7: 0x01 | (1 << 6) = 0x41.
  // Height 240 (0x00f0) with vert_scale = 3 (2/1) -> 240 * 2 = 480.
  // Byte 8: 0xf0, Byte 9: 0x00 | (3 << 6) = 0xc0.
  constexpr uint8_t kVp8ScaledKeyframeBuffer[] = {
      0x10, 0x00, 0x00, 0x9d, 0x01, 0x2a, 0x40, 0x41, 0xf0, 0xc0,
  };
  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kVp8ScaledKeyframeBuffer)));
  UNSAFE_BUFFERS(
      base::span(buffer->writable_data(), sizeof(kVp8ScaledKeyframeBuffer))
          .copy_from(base::span(kVp8ScaledKeyframeBuffer)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(400)),
                                  Field(&chromecast::Size::height, Eq(480)))))
      .Times(1);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 320);
  EXPECT_EQ(video_info->frame_height, 240);
}

TEST_F(StarboardVideoDecoderTest, InBandMultiStepResolutionSwitching) {
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP8;
  config.width = 1280;
  config.height = 720;

  MockDelegate delegate;
  StarboardVideoDecoder decoder(starboard_.get());
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Step 1: Switch to 320x240 (portrait/custom)
  constexpr uint8_t kFrame320x240[] = {
      0x10, 0x00, 0x00, 0x9d, 0x01, 0x2a, 0x40, 0x01, 0xf0, 0x00,
  };
  scoped_refptr<CastDecoderBufferImpl> buffer1(
      new CastDecoderBufferImpl(sizeof(kFrame320x240)));
  UNSAFE_BUFFERS(base::span(buffer1->writable_data(), sizeof(kFrame320x240))
                     .copy_from(base::span(kFrame320x240)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(320)),
                                  Field(&chromecast::Size::height, Eq(240)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(buffer1.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  // Step 2: Switch to 640x480 (landscape)
  // Width 640 (0x0280) -> byte 6: 0x80, byte 7: 0x02
  // Height 480 (0x01e0) -> byte 8: 0xe0, byte 9: 0x01
  constexpr uint8_t kFrame640x480[] = {
      0x10, 0x00, 0x00, 0x9d, 0x01, 0x2a, 0x80, 0x02, 0xe0, 0x01,
  };
  scoped_refptr<CastDecoderBufferImpl> buffer2(
      new CastDecoderBufferImpl(sizeof(kFrame640x480)));
  UNSAFE_BUFFERS(base::span(buffer2->writable_data(), sizeof(kFrame640x480))
                     .copy_from(base::span(kFrame640x480)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(640)),
                                  Field(&chromecast::Size::height, Eq(480)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(buffer2.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 640);
  EXPECT_EQ(video_info->frame_height, 480);
}

// A VP9 keyframe for 320x240 video.
constexpr uint8_t kVp9Keyframe320x240[] = {
    0x82, 0x49, 0x83, 0x42, 0x00, 0x13, 0xf0, 0x0e, 0xf6, 0x12, 0x38, 0x24,
    0x1c, 0x18, 0x82, 0x00, 0x07, 0x80, 0x7f, 0xd1, 0xfa, 0x3e, 0x8f, 0xe7,
    0x71, 0xfb, 0xfe, 0xfc, 0x2f, 0xf3, 0xbf, 0xce, 0xff, 0x67, 0xf3, 0x7e,
    0x7f, 0x8d, 0xf0, 0xfd, 0x1f, 0xac, 0xf7, 0x82, 0xfe, 0x5f, 0xc9, 0xfe,
    0x67, 0xc8, 0xff, 0xb8, 0xf7, 0x9b, 0xf7, 0xd9, 0xff, 0x6f, 0xe7, 0xff,
    0xc6, 0xfc, 0x2f, 0xd1, 0xf6, 0x77, 0xfc, 0x27, 0xc6, 0xff, 0xa7, 0xfb,
    0x77, 0xff, 0x09, 0xf0, 0xbf, 0xf0, 0x3f, 0xe7, 0xbe, 0xee, 0x3e, 0xce,
    0xff, 0x7b, 0xdb, 0x7f, 0x27, 0xc4, 0xf4, 0xbe, 0x9f, 0xde, 0x1b, 0xd1,
    0x75, 0xc7, 0xfb, 0xd1, 0xfb, 0xc6, 0x7b, 0xc3, 0x7a, 0x42, 0x72, 0xdc,
    0xec, 0x7c, 0x22, 0xc1, 0xee, 0xce, 0xdf, 0xec, 0x62, 0x07, 0x58, 0x2a,
    0x13, 0x45, 0x53, 0xda, 0x6f, 0x7e, 0x07, 0xc1, 0xb7, 0x60, 0x13, 0xba,
    0xf8, 0xa4, 0x01, 0x53, 0x20, 0x00, 0x7d, 0xd4, 0x37, 0x1d, 0xff, 0x6d,
    0x05, 0xab, 0x96, 0x6b, 0x3d, 0xcd, 0x78, 0x4b, 0x22, 0x83, 0x15, 0x17,
    0x84, 0x11, 0xc5, 0xac, 0x92, 0x81, 0xf1, 0x17, 0x72, 0xd7, 0xe7, 0xc6,
    0x04,
};

// Wraps `frames` into a VP9 Annex B superframe by appending a superframe index.
//
// The index is a marker byte, one little-endian size per frame, and a repeat of
// the marker byte. In the marker, bits 0-2 hold (frame count - 1) and bits 3-4
// hold (bytes per size - 1); the top three bits are always 0b110.
std::vector<uint8_t> BuildVp9Superframe(
    const std::vector<base::span<const uint8_t>>& frames) {
  constexpr size_t kBytesPerSize = 4;
  CHECK(!frames.empty());
  CHECK_LE(frames.size(), 8u);

  const uint8_t marker =
      0xc0 | ((kBytesPerSize - 1) << 3) | (frames.size() - 1);

  std::vector<uint8_t> superframe;
  for (const base::span<const uint8_t>& frame : frames) {
    superframe.insert(superframe.end(), frame.begin(), frame.end());
  }

  superframe.push_back(marker);
  for (const base::span<const uint8_t>& frame : frames) {
    for (size_t i = 0; i < kBytesPerSize; ++i) {
      superframe.push_back(
          static_cast<uint8_t>((frame.size() >> (8 * i)) & 0xff));
    }
  }
  superframe.push_back(marker);

  return superframe;
}

// A malformed frame inside a superframe must not stop the decoder from
// inspecting the frames that follow it.
//
// Beyond the resolution check, this exercises the requirement that the VP9
// parser is drained to kEOStream. Vp9Parser::FrameInfo holds a base::raw_span
// into the pushed buffer, which is freed once PushBuffer returns, so abandoning
// the parse early would leave the parser holding references to a freed
// allocation. Builds with dangling raw_ptr checks enabled CHECK-fail on that.
TEST_F(StarboardVideoDecoderTest,
       InBandVp9SuperframeWithMalformedLeadingFrameStillUpdatesResolution) {
  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP9;
  config.width = 1280;
  config.height = 720;
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // A VP9 uncompressed header must begin with the two-bit frame marker 0b10.
  // These bytes do not, so the parser rejects this frame with kInvalidStream.
  constexpr uint8_t kMalformedFrame[] = {0x00, 0x00, 0x00, 0x00};

  const std::vector<uint8_t> superframe = BuildVp9Superframe(
      {base::span(kMalformedFrame), base::span(kVp9Keyframe320x240)});

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(superframe.size()));
  // SAFETY: In unit test, buffer was allocated with exactly superframe.size().
  UNSAFE_BUFFERS(base::span(buffer->writable_data(), superframe.size())
                     .copy_from(base::span(superframe)));

  // The resolution from the second frame must still be reported.
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(320)),
                                  Field(&chromecast::Size::height, Eq(240)))))
      .Times(1);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 320);
  EXPECT_EQ(video_info->frame_height, 240);
}

// An in-band resolution belongs to the stream it was parsed from. Once the
// decoder is stopped and re-initialized for a new stream, it must go back to
// reporting the resolution the container declared.
TEST_F(StarboardVideoDecoderTest, ReinitializationRestoresContainerResolution) {
  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP9;
  config.width = 1280;
  config.height = 720;
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kVp9Keyframe320x240)));
  // SAFETY: In unit test, buffer has sufficient size for constant test array.
  UNSAFE_BUFFERS(
      base::span(buffer->writable_data(), sizeof(kVp9Keyframe320x240))
          .copy_from(base::span(kVp9Keyframe320x240)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(320)),
                                  Field(&chromecast::Size::height, Eq(240)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  {
    const auto& video_info = decoder.GetVideoSampleInfo();
    ASSERT_NE(video_info, std::nullopt);
    ASSERT_EQ(video_info->frame_width, 320);
    ASSERT_EQ(video_info->frame_height, 240);
  }

  // A new stream begins.
  decoder.Stop();
  decoder.Initialize(&fake_player_);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 1280);
  EXPECT_EQ(video_info->frame_height, 720);
}

// When the first video frame arrives before SbPlayer is created (a common case
// during stream startup where PushBuffer queues pending_first_push_), the
// in-band resolution detected from that frame must NOT be clobbered back to the
// container resolution when Initialize() subsequently runs.
TEST_F(StarboardVideoDecoderTest,
       PushBufferBeforeInitializationPreservesInBandResolution) {
  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP9;
  config.width = 1280;
  config.height = 720;
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Push buffer BEFORE decoder.Initialize() has been called (player is null).
  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kVp9Keyframe320x240)));
  // SAFETY: In unit test, buffer has sufficient size for constant test array.
  UNSAFE_BUFFERS(
      base::span(buffer->writable_data(), sizeof(kVp9Keyframe320x240))
          .copy_from(base::span(kVp9Keyframe320x240)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(320)),
                                  Field(&chromecast::Size::height, Eq(240)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  {
    const auto& video_info = decoder.GetVideoSampleInfo();
    ASSERT_NE(video_info, std::nullopt);
    EXPECT_EQ(video_info->frame_width, 320);
    EXPECT_EQ(video_info->frame_height, 240);
  }

  // SbPlayer is created; Initialize() runs. The detected resolution must
  // survive.
  EXPECT_CALL(*starboard_, WriteSample).Times(1);
  decoder.Initialize(&fake_player_);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 320);
  EXPECT_EQ(video_info->frame_height, 240);
}

class Vp9HeaderBitWriter {
 public:
  void WriteBits(uint32_t value, int num_bits) {
    for (int i = num_bits - 1; i >= 0; --i) {
      WriteBit((value >> i) & 1u);
    }
  }

  void WriteBit(uint32_t bit) {
    if (bit_offset_ == 0) {
      bytes_.push_back(0);
    }
    if (bit & 1u) {
      bytes_.back() |= static_cast<uint8_t>(1u << (7 - bit_offset_));
    }
    bit_offset_ = (bit_offset_ + 1) % 8;
  }

  void AlignToByteWithZeros() {
    while (bit_offset_ != 0) {
      WriteBit(0);
    }
  }

  std::vector<uint8_t> FinishWithOneByteCompressedHeader() {
    AlignToByteWithZeros();
    // Append 1 zero byte to satisfy header_size_in_bytes = 1.
    bytes_.push_back(0x00);
    return std::move(bytes_);
  }

 private:
  std::vector<uint8_t> bytes_;
  int bit_offset_ = 0;
};

std::vector<uint8_t> BuildVp9KeyframeHeader(int coded_width,
                                            int coded_height,
                                            int render_width,
                                            int render_height,
                                            bool show_frame = true) {
  Vp9HeaderBitWriter w;
  w.WriteBits(0x2, 2);                // frame_marker = 2
  w.WriteBits(0x0, 2);                // profile = 0
  w.WriteBit(0);                      // show_existing_frame = 0
  w.WriteBit(0);                      // frame_type = 0 (KEYFRAME)
  w.WriteBit(show_frame ? 1 : 0);     // show_frame
  w.WriteBit(1);                      // error_resilient_mode = 1
  w.WriteBits(0x498342, 24);          // sync_code
  w.WriteBits(1, 3);                  // color_space = BT_601 (1)
  w.WriteBit(0);                      // color_range = 0
  w.WriteBits(coded_width - 1, 16);   // frame_width - 1
  w.WriteBits(coded_height - 1, 16);  // frame_height - 1
  const bool has_custom_render =
      (render_width != coded_width || render_height != coded_height);
  w.WriteBit(has_custom_render ? 1 : 0);
  if (has_custom_render) {
    w.WriteBits(render_width - 1, 16);
    w.WriteBits(render_height - 1, 16);
  }
  w.WriteBits(0, 2);  // frame_context_idx = 0
  w.WriteBits(0, 6);  // loop_filter_level = 0
  w.WriteBits(0, 3);  // loop_filter_sharpness = 0
  w.WriteBit(0);      // loop_filter_delta_enabled = 0
  w.WriteBits(0, 8);  // base_q_idx = 0
  w.WriteBits(0, 3);  // delta_q_y_dc, delta_q_uv_dc, delta_q_uv_ac = 0
  w.WriteBit(0);      // segmentation_enabled = 0
  const int sb64_cols = (coded_width + 63) / 64;
  int min_log2 = 0;
  while ((64 << min_log2) < sb64_cols) {
    ++min_log2;
  }
  int max_log2 = 1;
  while ((sb64_cols >> max_log2) >= 4) {
    ++max_log2;
  }
  --max_log2;
  if (max_log2 > min_log2) {
    w.WriteBit(0);  // tile_cols_log2 increment = 0
  }
  w.WriteBit(0);       // tile_rows_log2 = 0
  w.WriteBits(1, 16);  // header_size_in_bytes = 1
  return w.FinishWithOneByteCompressedHeader();
}

std::vector<uint8_t> BuildVp9InterframeHeader(int coded_width,
                                              int coded_height,
                                              int render_width,
                                              int render_height,
                                              uint8_t refresh_flags = 0x00) {
  Vp9HeaderBitWriter w;
  w.WriteBits(0x2, 2);            // frame_marker = 2
  w.WriteBits(0x0, 2);            // profile = 0
  w.WriteBit(0);                  // show_existing_frame = 0
  w.WriteBit(1);                  // frame_type = 1 (INTERFRAME)
  w.WriteBit(1);                  // show_frame = 1
  w.WriteBit(1);                  // error_resilient_mode = 1
  w.WriteBits(refresh_flags, 8);  // refresh_frame_flags
  for (int i = 0; i < 3; ++i) {
    w.WriteBits(0, 3);  // ref_frame_idx[i] = 0
    w.WriteBit(0);      // ref_frame_sign_bias[i] = 0
  }
  w.WriteBits(0, 3);                  // found_ref[0..2] = 0, 0, 0
  w.WriteBits(coded_width - 1, 16);   // frame_width - 1
  w.WriteBits(coded_height - 1, 16);  // frame_height - 1
  const bool has_custom_render =
      (render_width != coded_width || render_height != coded_height);
  w.WriteBit(has_custom_render ? 1 : 0);
  if (has_custom_render) {
    w.WriteBits(render_width - 1, 16);
    w.WriteBits(render_height - 1, 16);
  }
  w.WriteBit(0);      // allow_high_precision_mv = 0
  w.WriteBit(1);      // is_filter_switchable = 1
  w.WriteBits(0, 2);  // frame_context_idx = 0
  w.WriteBits(0, 6);  // loop_filter_level = 0
  w.WriteBits(0, 3);  // loop_filter_sharpness = 0
  w.WriteBit(0);      // loop_filter_delta_enabled = 0
  w.WriteBits(0, 8);  // base_q_idx = 0
  w.WriteBits(0, 3);  // delta_q_y_dc, delta_q_uv_dc, delta_q_uv_ac = 0
  w.WriteBit(0);      // segmentation_enabled = 0
  const int sb64_cols = (coded_width + 63) / 64;
  int min_log2 = 0;
  while ((64 << min_log2) < sb64_cols) {
    ++min_log2;
  }
  int max_log2 = 1;
  while ((sb64_cols >> max_log2) >= 4) {
    ++max_log2;
  }
  --max_log2;
  if (max_log2 > min_log2) {
    w.WriteBit(0);  // tile_cols_log2 increment = 0
  }
  w.WriteBit(0);       // tile_rows_log2 = 0
  w.WriteBits(1, 16);  // header_size_in_bytes = 1
  return w.FinishWithOneByteCompressedHeader();
}

TEST_F(StarboardVideoDecoderTest,
       StopRestoresContainerResolutionEvenWhenEosPushedBeforeReinitialize) {
  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP9;
  config.width = 1280;
  config.height = 720;
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  const std::vector<uint8_t> keyframe =
      BuildVp9KeyframeHeader(320, 240, 320, 240);
  scoped_refptr<CastDecoderBufferImpl> kf_buffer(
      new CastDecoderBufferImpl(keyframe.size()));
  UNSAFE_BUFFERS(base::span(kf_buffer->writable_data(), keyframe.size())
                     .copy_from(base::span(keyframe)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(320)),
                                  Field(&chromecast::Size::height, Eq(240)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(kf_buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  // Stop the current stream, push an EOS buffer while stopped (which populates
  // pending_first_push_), and re-initialize. The previous stream's 320x240
  // in-band override must have been cleared, notifying the delegate of the
  // restored 1280x720 container resolution on the next push.
  decoder.Stop();
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(1280)),
                                  Field(&chromecast::Size::height, Eq(720)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(CastDecoderBufferImpl::CreateEOSBuffer().get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  EXPECT_CALL(*starboard_,
              WriteEndOfStream(&fake_player_, kStarboardMediaTypeVideo))
      .Times(1);
  decoder.Initialize(&fake_player_);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 1280);
  EXPECT_EQ(video_info->frame_height, 720);
}

TEST_F(StarboardVideoDecoderTest,
       InBandVp9RenderSizeAndInterframePreserveCodedSampleInfo) {
  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP9;
  config.width = 1280;
  config.height = 720;
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // 1. Push a VP9 keyframe with coded size 320x240 and render size 640x480.
  // Delegate must receive display size (640, 480), whereas Starboard sample
  // info must carry coded size (320, 240) so VpxVideoDecoder allocates and
  // copies the exact coded plane dimensions.
  const std::vector<uint8_t> keyframe =
      BuildVp9KeyframeHeader(320, 240, 640, 480);
  scoped_refptr<CastDecoderBufferImpl> kf_buffer(
      new CastDecoderBufferImpl(keyframe.size()));
  UNSAFE_BUFFERS(base::span(kf_buffer->writable_data(), keyframe.size())
                     .copy_from(base::span(keyframe)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(640)),
                                  Field(&chromecast::Size::height, Eq(480)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(kf_buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  {
    const auto& video_info = decoder.GetVideoSampleInfo();
    ASSERT_NE(video_info, std::nullopt);
    EXPECT_EQ(video_info->frame_width, 320);
    EXPECT_EQ(video_info->frame_height, 240);
  }

  // 2. Push a VP9 show_existing_frame referencing slot 0 (0x88) and an
  // interframe. Stateless keyframe-only parsing ignores non-keyframes so
  // Starboard sample info remains at (320, 240) without tearing down libvpx.
  constexpr uint8_t kShowExistingSlot0[] = {0x88};
  scoped_refptr<CastDecoderBufferImpl> sef_buffer(
      new CastDecoderBufferImpl(sizeof(kShowExistingSlot0)));
  UNSAFE_BUFFERS(
      base::span(sef_buffer->writable_data(), sizeof(kShowExistingSlot0))
          .copy_from(base::span(kShowExistingSlot0)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged).Times(0);
  EXPECT_EQ(decoder.PushBuffer(sef_buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const std::vector<uint8_t> interframe =
      BuildVp9InterframeHeader(480, 360, 480, 360);
  scoped_refptr<CastDecoderBufferImpl> inter_buffer(
      new CastDecoderBufferImpl(interframe.size()));
  UNSAFE_BUFFERS(base::span(inter_buffer->writable_data(), interframe.size())
                     .copy_from(base::span(interframe)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged).Times(0);
  EXPECT_EQ(decoder.PushBuffer(inter_buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  {
    const auto& video_info = decoder.GetVideoSampleInfo();
    ASSERT_NE(video_info, std::nullopt);
    EXPECT_EQ(video_info->frame_width, 320);
    EXPECT_EQ(video_info->frame_height, 240);
  }
}

TEST_F(StarboardVideoDecoderTest,
       InBandH264ContinuesPastMalformedSpsToValidSpsBeforeSlice) {
  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  VideoConfig config = GetBasicConfig();
  config.codec = kCodecH264;
  config.width = 1280;
  config.height = 720;
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Access unit containing a truncated/malformed SPS NALU followed by a valid
  // 320x240 SPS NALU and an IDR slice NALU.
  const uint8_t kMultiSpsBuffer[] = {
      // Truncated SPS NALU (nal_unit_type = 7):
      0x00,
      0x00,
      0x00,
      0x01,
      0x27,
      0x4d,
      // Valid 320x240 SPS NALU:
      0x00,
      0x00,
      0x00,
      0x01,
      0x27,
      0x4d,
      0x40,
      0x0d,
      0xa9,
      0x18,
      0x28,
      0x3e,
      0x60,
      0x0d,
      0x41,
      0x80,
      0x41,
      0xad,
      0xb0,
      0xad,
      0x7b,
      0xdf,
      0x01,
      0x00,
      // IDR slice NALU (nal_unit_type = 5):
      0x00,
      0x00,
      0x00,
      0x01,
      0x65,
      0x88,
      0x84,
      0x00,
  };

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kMultiSpsBuffer)));
  UNSAFE_BUFFERS(base::span(buffer->writable_data(), sizeof(kMultiSpsBuffer))
                     .copy_from(base::span(kMultiSpsBuffer)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(320)),
                                  Field(&chromecast::Size::height, Eq(240)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 320);
  EXPECT_EQ(video_info->frame_height, 240);
}

TEST_F(StarboardVideoDecoderTest,
       InBandH264StopsScanningAtFirstVclSliceWithoutScanningPayload) {
  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  VideoConfig config = GetBasicConfig();
  config.codec = kCodecH264;
  config.width = 1280;
  config.height = 720;
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Flush the initial SetConfig() resolution notification.
  scoped_refptr<CastDecoderBufferImpl> init_buffer(
      new CastDecoderBufferImpl(10));
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(1280)),
                                  Field(&chromecast::Size::height, Eq(720)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(init_buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  // Buffer beginning with a non-IDR VCL slice (nal_unit_type = 1) followed by
  // bytes that match a 320x240 SPS NALU inside the slice payload. Because ITU-T
  // H.264 7.4.1.2.3 forbids SPS NALUs after VCL slices in an access unit,
  // ParseH264Resolution must stop at the VCL slice header without scanning the
  // slice payload.
  const uint8_t kSliceWithEmbeddedSpsBytes[] = {
      0x00, 0x00, 0x00, 0x01, 0x01, 0x9a, 0x00, 0x00, 0x00, 0x01,
      0x27, 0x4d, 0x40, 0x0d, 0xa9, 0x18, 0x28, 0x3e, 0x60, 0x0d,
      0x41, 0x80, 0x41, 0xad, 0xb0, 0xad, 0x7b, 0xdf, 0x01, 0x00,
  };

  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kSliceWithEmbeddedSpsBytes)));
  UNSAFE_BUFFERS(
      base::span(buffer->writable_data(), sizeof(kSliceWithEmbeddedSpsBytes))
          .copy_from(base::span(kSliceWithEmbeddedSpsBytes)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged).Times(0);
  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 1280);
  EXPECT_EQ(video_info->frame_height, 720);
}

TEST_F(StarboardVideoDecoderTest,
       InBandVp9InvisibleKeyframeUpdatesDisplayAndCodedResolution) {
  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP9;
  config.width = 1280;
  config.height = 720;
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Build an invisible VP9 keyframe (show_frame = false) with coded size
  // 320x240 and render size 640x480.
  const std::vector<uint8_t> invisible_keyframe =
      BuildVp9KeyframeHeader(320, 240, 640, 480, /*show_frame=*/false);
  scoped_refptr<CastDecoderBufferImpl> kf_buffer(
      new CastDecoderBufferImpl(invisible_keyframe.size()));
  UNSAFE_BUFFERS(
      base::span(kf_buffer->writable_data(), invisible_keyframe.size())
          .copy_from(base::span(invisible_keyframe)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(640)),
                                  Field(&chromecast::Size::height, Eq(480)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(kf_buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 320);
  EXPECT_EQ(video_info->frame_height, 240);
}

TEST_F(StarboardVideoDecoderTest,
       PreInitPushFollowedByStopOrEosRestoresContainerResolution) {
  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP9;
  config.width = 1280;
  config.height = 720;
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // 1. Push a 320x240 keyframe BEFORE Initialize(), then call Stop() before
  // Initialize() ever runs. When Initialize() subsequently runs, the dropped
  // pre-init frame's 320x240 override must be reverted to the 1280x720
  // container resolution.
  scoped_refptr<CastDecoderBufferImpl> buffer(
      new CastDecoderBufferImpl(sizeof(kVp9Keyframe320x240)));
  UNSAFE_BUFFERS(
      base::span(buffer->writable_data(), sizeof(kVp9Keyframe320x240))
          .copy_from(base::span(kVp9Keyframe320x240)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(320)),
                                  Field(&chromecast::Size::height, Eq(240)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  decoder.Stop();
  decoder.Initialize(&fake_player_);

  {
    const auto& video_info = decoder.GetVideoSampleInfo();
    ASSERT_NE(video_info, std::nullopt);
    EXPECT_EQ(video_info->frame_width, 1280);
    EXPECT_EQ(video_info->frame_height, 720);
  }

  // 2. Stop again, push a 320x240 keyframe BEFORE Initialize(), and then push
  // EOS BEFORE Initialize() (which drops the queued keyframe). The container
  // resolution (1280x720) must be restored immediately.
  decoder.Stop();
  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(320)),
                                  Field(&chromecast::Size::height, Eq(240)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(1280)),
                                  Field(&chromecast::Size::height, Eq(720)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(CastDecoderBufferImpl::CreateEOSBuffer().get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 1280);
  EXPECT_EQ(video_info->frame_height, 720);
}

TEST_F(StarboardVideoDecoderTest,
       Negotiated720pContainerHandlesInBandPortraitMirroringAndRotation) {
  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;

  // Stage 1 (OpenScreen ANSWER clamping): session negotiates 1280x720 ceiling
  // for a 720p display, initializing container_width_=1280,
  // container_height_=720.
  VideoConfig config = GetBasicConfig();
  config.codec = kCodecVP9;
  config.width = 1280;
  config.height = 720;
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // Stage 2 (In-band portrait mirroring / rotation with "scaling": "receiver"):
  // Sender streams a 576x1024 portrait frame whose height (1024) exceeds the
  // initial 720p landscape container_height_, followed by rotation to 1280x720.
  const std::vector<uint8_t> portrait_kf =
      BuildVp9KeyframeHeader(576, 1024, 576, 1024);
  scoped_refptr<CastDecoderBufferImpl> portrait_buf(
      new CastDecoderBufferImpl(portrait_kf.size()));
  UNSAFE_BUFFERS(base::span(portrait_buf->writable_data(), portrait_kf.size())
                     .copy_from(base::span(portrait_kf)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(576)),
                                  Field(&chromecast::Size::height, Eq(1024)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(portrait_buf.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  {
    const auto& video_info = decoder.GetVideoSampleInfo();
    ASSERT_NE(video_info, std::nullopt);
    EXPECT_EQ(video_info->frame_width, 576);
    EXPECT_EQ(video_info->frame_height, 1024);
  }

  // Rotate back to 1280x720 landscape in-band.
  const std::vector<uint8_t> landscape_kf =
      BuildVp9KeyframeHeader(1280, 720, 1280, 720);
  scoped_refptr<CastDecoderBufferImpl> landscape_buf(
      new CastDecoderBufferImpl(landscape_kf.size()));
  UNSAFE_BUFFERS(base::span(landscape_buf->writable_data(), landscape_kf.size())
                     .copy_from(base::span(landscape_kf)));

  EXPECT_CALL(delegate, OnVideoResolutionChanged(
                            AllOf(Field(&chromecast::Size::width, Eq(1280)),
                                  Field(&chromecast::Size::height, Eq(720)))))
      .Times(1);
  EXPECT_EQ(decoder.PushBuffer(landscape_buf.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  const auto& video_info = decoder.GetVideoSampleInfo();
  ASSERT_NE(video_info, std::nullopt);
  EXPECT_EQ(video_info->frame_width, 1280);
  EXPECT_EQ(video_info->frame_height, 720);
}

// Builds an encrypted 10-byte buffer whose key ID is `key_id`. Callers must
// register `key_id` with StarboardDrmKeyTracker before the buffer can be pushed
// to starboard.
scoped_refptr<DecoderBufferAdapter> CreateEncryptedBuffer(const char* key_id) {
  // This must be 16 bytes.
  constexpr char kIv[] = "abcdefghijklmnop";
  // The subsamples account for 1+2+3+4 = 10 bytes, so the data must be exactly
  // 10 bytes for DecoderBufferAdapter to keep the buffer's decrypt config.
  const std::vector<::media::SubsampleEntry> subsamples = {
      ::media::SubsampleEntry(/*clear_bytes=*/1, /*cypher_bytes=*/2),
      ::media::SubsampleEntry(/*clear_bytes=*/3, /*cypher_bytes=*/4),
  };
  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCbcsConfig(
          key_id, kIv, subsamples, ::media::EncryptionPattern::Create(5, 6));
  CHECK(decrypt_config);

  const std::vector<uint8_t> buffer_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  scoped_refptr<::media::DecoderBuffer> decoder_buffer =
      ::media::DecoderBuffer::CopyFrom(buffer_data);
  CHECK(decoder_buffer);
  decoder_buffer->set_decrypt_config(std::move(decrypt_config));
  return base::MakeRefCounted<DecoderBufferAdapter>(decoder_buffer);
}

TEST_F(StarboardVideoDecoderTest,
       OnBufferWrittenWhileWaitingForDrmKeyKeepsPendingPush) {
  // While an encrypted buffer is parked waiting for its DRM key, starboard may
  // report NeedsData again for the previously consumed sample. That must not
  // discard the parked push: the key-available callback is still registered
  // and runs the parked push once the key arrives.
  constexpr char kKeyId[] = "key_id";
  VideoConfig config = GetBasicConfig();
  config.encryption_scheme = EncryptionScheme::kAesCbc;
  scoped_refptr<DecoderBufferAdapter> buffer = CreateEncryptedBuffer(kKeyId);

  StarboardVideoDecoder decoder(starboard_.get());
  MockDelegate delegate;
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  // The key is not available yet, so nothing is written to starboard.
  EXPECT_CALL(*starboard_, WriteSample).Times(0);
  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);
  Mock::VerifyAndClearExpectations(starboard_.get());

  EXPECT_CALL(delegate, OnPushBufferComplete).Times(AnyNumber());
  decoder.OnBufferWritten();

  // Once the key arrives, the parked buffer must be written exactly once.
  EXPECT_CALL(
      *starboard_,
      WriteSample(&fake_player_, kStarboardMediaTypeVideo,
                  ElementsAre(MatchesVideoConfigAndBuffer(config, buffer))))
      .Times(1);
  StarboardDrmKeyTracker::GetInstance().AddKey(kKeyId, "session_id");
  // The key-available callback is posted to this sequence.
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace media
}  // namespace chromecast
