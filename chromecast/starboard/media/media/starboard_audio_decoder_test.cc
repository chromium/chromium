// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/starboard_audio_decoder.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "base/logging.h"
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
using ::testing::FloatEq;
using ::testing::MockFunction;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Pointwise;
using ::testing::StrEq;
using ::testing::WithArg;

// Converts between AudioCodec and StarboardAudioCodec.
StarboardAudioCodec ToStarboardAudioCodec(AudioCodec codec) {
  switch (codec) {
    case kCodecAAC:
      return kStarboardAudioCodecAac;
    case kCodecMP3:
      return kStarboardAudioCodecMp3;
    case kCodecPCM:
      return kStarboardAudioCodecPcm;
    case kCodecVorbis:
      return kStarboardAudioCodecVorbis;
    case kCodecOpus:
      return kStarboardAudioCodecOpus;
    case kCodecEAC3:
      return kStarboardAudioCodecEac3;
    case kCodecAC3:
      return kStarboardAudioCodecAc3;
    case kCodecFLAC:
      return kStarboardAudioCodecFlac;
    // The rest of these codecs are currently unsupported by starboard.
    case kCodecDTS:
    case kCodecPCM_S16BE:
    case kCodecMpegHAudio:
    default:
      return kStarboardAudioCodecNone;
  }
}

// Returns the number of bits per sample per channel for the given sample
// format.
uint16_t GetBitsPerSample(SampleFormat sample_format) {
  switch (sample_format) {
    case kSampleFormatU8:
      return 8;
    case kSampleFormatS16:
    case kSampleFormatPlanarS16:
      return 16;
    case kSampleFormatS24:
      return 24;
    case kSampleFormatS32:
    case kSampleFormatF32:
    case kSampleFormatPlanarF32:
    case kSampleFormatPlanarS32:
      return 32;
    default:
      return 0;
  }
}

// Compares a StarboardSampleInfo (arg) to an AudioConfig (config) and a
// scoped_refptr<CastDecoderBuffer> (buffer).
MATCHER_P2(MatchesAudioConfigAndBuffer, config, buffer, "") {
  CHECK(buffer) << "Passed a null buffer to the matcher.";

  if (arg.type != kStarboardMediaTypeAudio) {
    *result_listener << "the StarboardSampleInfo's type is not audio";
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

  // Check the rest of the fields.
  return ExplainMatchResult(Eq(buffer->timestamp()), arg.timestamp,
                            result_listener) &&
         ExplainMatchResult(
             AllOf(Field(&StarboardAudioSampleInfo::codec,
                         ToStarboardAudioCodec(config.codec)),
                   Field(&StarboardAudioSampleInfo::number_of_channels,
                         config.channel_number),
                   Field(&StarboardAudioSampleInfo::samples_per_second,
                         config.samples_per_second),
                   Field(&StarboardAudioSampleInfo::bits_per_sample,
                         GetBitsPerSample(config.sample_format))),
             arg.audio_sample_info, result_listener);
  return true;
}

// Compares a StarboardSampleInfo (arg) to a
// scoped_refptr<CastDecoderBuffer> (buffer).
MATCHER_P(MatchesAudioBufferPCM, buffer, "") {
  CHECK(buffer) << "Passed a null buffer to the matcher.";
  if (arg.type != kStarboardMediaTypeAudio) {
    *result_listener << "the StarboardSampleInfo's type is not audio";
    return false;
  }

  CHECK_EQ(static_cast<int>(buffer->data_size()) % 4, 0);
  CHECK_EQ(arg.buffer_size % 4, 0);

  const std::vector<float> expected_buffer(
      reinterpret_cast<const float*>(buffer->data()),
      reinterpret_cast<const float*>(buffer->data()) + buffer->data_size() / 4);

  if (!ExplainMatchResult(
          Pointwise(FloatEq(), expected_buffer),
          std::tuple<const float*, size_t>(
              reinterpret_cast<const float*>(arg.buffer), arg.buffer_size / 4),
          result_listener)) {
    *result_listener << "buffer data mismatch";
    return false;
  }
  return true;
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
class StarboardAudioDecoderTest : public ::testing::Test {
 protected:
  StarboardAudioDecoderTest()
      : starboard_(std::make_unique<MockStarboardApiWrapper>()) {
    // Ensure that tests begin with a clean slate regarding DRM keys.
    StarboardDrmKeyTracker::GetInstance().ClearStateForTesting();
  }

  ~StarboardAudioDecoderTest() override = default;

  // This should be destructed last.
  base::test::SingleThreadTaskEnvironment task_environment_;
  // This will be passed to the MediaPipelineBackendStarboard, and all calls to
  // Starboard will go through it. Thus, we can mock out those calls.
  std::unique_ptr<MockStarboardApiWrapper> starboard_;
  // Since SbPlayer is just an opaque blob to the MPB, we will simply use an int
  // to represent it.
  int fake_player_ = 1;
};

// Returns a simple AudioConfig.
AudioConfig GetBasicConfig() {
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

TEST_F(StarboardAudioDecoderTest, PushesBufferToStarboard) {
  const AudioConfig config = GetBasicConfig();
  const std::vector<uint8_t> buffer_data = {1, 2, 3, 4, 5};
  auto buffer = base::MakeRefCounted<DecoderBufferAdapter>(
      ::media::DecoderBuffer::CopyFrom(buffer_data));

  EXPECT_CALL(
      *starboard_,
      WriteSample(&fake_player_, kStarboardMediaTypeAudio,
                  ElementsAre(MatchesAudioConfigAndBuffer(config, buffer))))
      .Times(1);

  StarboardAudioDecoder decoder(starboard_.get());
  MockDelegate delegate;

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);
}

TEST_F(StarboardAudioDecoderTest, WritesEndOfStreamToStarboard) {
  EXPECT_CALL(*starboard_,
              WriteEndOfStream(&fake_player_, kStarboardMediaTypeAudio))
      .Times(1);

  StarboardAudioDecoder decoder(starboard_.get());
  MockDelegate delegate;

  const AudioConfig config = GetBasicConfig();

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  EXPECT_EQ(decoder.PushBuffer(CastDecoderBufferImpl::CreateEOSBuffer().get()),
            MediaPipelineBackend::BufferStatus::kBufferSuccess);
}

TEST_F(StarboardAudioDecoderTest,
       EndOfStreamAfterStopDoesNotPushToNullSbPlayer) {
  // Regression test for crbug.com/375652489.
  EXPECT_CALL(*starboard_, WriteEndOfStream(nullptr, kStarboardMediaTypeAudio))
      .Times(0);
  EXPECT_CALL(*starboard_,
              WriteEndOfStream(&fake_player_, kStarboardMediaTypeAudio))
      .Times(1);

  StarboardAudioDecoder decoder(starboard_.get());
  MockDelegate delegate;

  const AudioConfig config = GetBasicConfig();

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);
  decoder.Stop();

  EXPECT_EQ(decoder.PushBuffer(CastDecoderBufferImpl::CreateEOSBuffer().get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  // This should trigger the WriteEndOfStream call.
  decoder.Initialize(&fake_player_);
}

TEST_F(StarboardAudioDecoderTest, ForwardsSetVolumeCallToStarboard) {
  constexpr float kVolume = 0.77;

  EXPECT_CALL(*starboard_, SetVolume(&fake_player_, DoubleEq(kVolume)))
      .Times(1);

  StarboardAudioDecoder decoder(starboard_.get());
  MockDelegate delegate;

  const AudioConfig config = GetBasicConfig();

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  EXPECT_TRUE(decoder.SetVolume(kVolume));
}

TEST_F(StarboardAudioDecoderTest, PopulatesDrmInfoInSamples) {
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

  AudioConfig config = GetBasicConfig();
  // Match the behavior of AudioPipelineImpl::Initialize by setting this to
  // unencrypted even for encrypted content.
  config.encryption_scheme = EncryptionScheme::kUnencrypted;

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
      WriteSample(
          &fake_player_, kStarboardMediaTypeAudio,
          ElementsAre(AllOf(MatchesAudioConfigAndBuffer(config, buffer)))))
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

  StarboardAudioDecoder decoder(starboard_.get());
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

TEST_F(StarboardAudioDecoderTest, DoesNotPushToStarboardIfDrmKeyIsUnavailable) {
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

  AudioConfig config = GetBasicConfig();
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

  StarboardAudioDecoder decoder(starboard_.get());
  MockDelegate delegate;

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);
}

TEST_F(StarboardAudioDecoderTest,
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

  AudioConfig config = GetBasicConfig();
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
      WriteSample(
          &fake_player_, kStarboardMediaTypeAudio,
          ElementsAre(AllOf(MatchesAudioConfigAndBuffer(config, buffer)))))
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

  StarboardAudioDecoder decoder(starboard_.get());
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

TEST_F(StarboardAudioDecoderTest, ReturnsNulloptBeforeConfigSet) {
  StarboardAudioDecoder decoder(starboard_.get());

  EXPECT_EQ(decoder.GetAudioSampleInfo(), std::nullopt);
}

TEST_F(StarboardAudioDecoderTest,
       ReturnsPopulatedSampleInfoAfterConfigHasBeenSet) {
  StarboardAudioDecoder decoder(starboard_.get());

  AudioConfig config;
  config.codec = AudioCodec::kCodecAAC;
  config.channel_layout = ChannelLayout::SURROUND_5_1;
  config.sample_format = SampleFormat::kSampleFormatF32;
  config.bytes_per_channel = 4;
  config.channel_number = 6;
  config.samples_per_second = 48000;
  config.encryption_scheme = EncryptionScheme::kUnencrypted;

  EXPECT_TRUE(decoder.SetConfig(config));
  EXPECT_THAT(decoder.GetAudioSampleInfo(),
              Optional(Field(&StarboardAudioSampleInfo::codec,
                             kStarboardAudioCodecAac)));
}

TEST_F(StarboardAudioDecoderTest,
       HandlesMultiplePushBuffersBeforeInitialization) {
  const std::vector<uint8_t> buffer_data_1 = {1, 2, 3, 4, 5};
  auto buffer_1 = base::MakeRefCounted<DecoderBufferAdapter>(
      ::media::DecoderBuffer::CopyFrom(buffer_data_1));

  const std::vector<uint8_t> buffer_data_2 = {6, 7, 8, 9, 10};
  auto buffer_2 = base::MakeRefCounted<DecoderBufferAdapter>(
      ::media::DecoderBuffer::CopyFrom(buffer_data_2));

  const AudioConfig config = GetBasicConfig();

  // Only the last buffer -- buffer_2 -- should be sent to starboard once the
  // decoder is initialized.
  EXPECT_CALL(
      *starboard_,
      WriteSample(&fake_player_, kStarboardMediaTypeAudio,
                  ElementsAre(MatchesAudioConfigAndBuffer(config, buffer_1))))
      .Times(0);
  EXPECT_CALL(
      *starboard_,
      WriteSample(&fake_player_, kStarboardMediaTypeAudio,
                  ElementsAre(MatchesAudioConfigAndBuffer(config, buffer_2))))
      .Times(1);

  StarboardAudioDecoder decoder(starboard_.get());
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

TEST_F(StarboardAudioDecoderTest, DoesNotCallDelegateEoSWhenPushingEoSBuffer) {
  const AudioConfig config = GetBasicConfig();

  StarboardAudioDecoder decoder(starboard_.get());

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

TEST_F(StarboardAudioDecoderTest, CallsDelegateEoSWhenSbPlayerStreamEnds) {
  const AudioConfig config = GetBasicConfig();

  StarboardAudioDecoder decoder(starboard_.get());

  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  MockDelegate delegate;
  EXPECT_CALL(delegate, OnEndOfStream).Times(1);
  decoder.SetDelegate(&delegate);
  decoder.OnSbPlayerEndOfStream();
}

TEST_F(StarboardAudioDecoderTest, ReportsStatistics) {
  StarboardAudioDecoder decoder(starboard_.get());
  MockDelegate delegate;

  const AudioConfig config = GetBasicConfig();
  decoder.Initialize(&fake_player_);
  decoder.SetConfig(config);
  decoder.SetDelegate(&delegate);

  const std::vector<uint8_t> buffer_data = {1, 2, 3, 4, 5};
  auto buffer = base::MakeRefCounted<DecoderBufferAdapter>(
      ::media::DecoderBuffer::CopyFrom(buffer_data));

  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  MediaPipelineBackend::AudioDecoder::Statistics stats = {};
  decoder.GetStatistics(&stats);
  EXPECT_EQ(stats.decoded_bytes, buffer_data.size());
}

TEST_F(StarboardAudioDecoderTest, ConvertsPcmToS16ForPushBeforeInitialization) {
  // NOTE: this test relies on cast converting PCM data to S16. If the code is
  // updated to read an output format from partners, we should update this test
  // correspondingly (to report S16 as the desired output format).

  // This will be treated as unsigned 8 bit samples, and we expect it to be
  // converted to two S16 samples.
  const std::vector<uint8_t> buffer_data = {0x00, 0xFF};
  auto buffer = base::MakeRefCounted<DecoderBufferAdapter>(
      ::media::DecoderBuffer::CopyFrom(buffer_data));

  AudioConfig original_config;
  original_config.codec = AudioCodec::kCodecPCM;
  original_config.channel_layout = ChannelLayout::MONO;
  original_config.sample_format = SampleFormat::kSampleFormatU8;
  original_config.bytes_per_channel = 1;
  original_config.channel_number = 1;
  original_config.samples_per_second = 44100;
  original_config.encryption_scheme = EncryptionScheme::kUnencrypted;

  AudioConfig resampled_config;
  resampled_config.codec = AudioCodec::kCodecPCM;
  resampled_config.channel_layout = ChannelLayout::MONO;
  resampled_config.sample_format = SampleFormat::kSampleFormatS16;
  resampled_config.bytes_per_channel = 2;
  resampled_config.channel_number = 1;
  resampled_config.samples_per_second = 44100;
  resampled_config.encryption_scheme = EncryptionScheme::kUnencrypted;

  // Note: this is little endian representing two S16 values corresponding to
  // buffer_data above.
  const std::vector<uint8_t> expected_resampled_buffer_data = {0x00, 0x80, 0xFF,
                                                               0x7F};
  auto expected_resampled_buffer = base::MakeRefCounted<DecoderBufferAdapter>(
      ::media::DecoderBuffer::CopyFrom(expected_resampled_buffer_data));

  EXPECT_CALL(*starboard_,
              WriteSample(&fake_player_, kStarboardMediaTypeAudio,
                          ElementsAre(MatchesAudioConfigAndBuffer(
                              resampled_config, expected_resampled_buffer))))
      .Times(1);

  StarboardAudioDecoder decoder(starboard_.get());
  decoder.SetConfig(original_config);
  EXPECT_EQ(decoder.PushBuffer(buffer.get()),
            MediaPipelineBackend::BufferStatus::kBufferPending);

  MockDelegate delegate;
  decoder.SetDelegate(&delegate);

  // At this point, the pending buffer should be pushed.
  decoder.Initialize(&fake_player_);
}

}  // namespace
}  // namespace media
}  // namespace chromecast
