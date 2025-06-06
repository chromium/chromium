// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/demuxer_stream_reader.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromecast/starboard/media/cdm/starboard_drm_key_tracker.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/media/media/test_matchers.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/encryption_scheme.h"
#include "media/base/mock_filters.h"
#include "media/base/sample_format.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::base::test::SingleThreadTaskEnvironment;
using ::media::AudioDecoderConfig;
using ::media::DecoderBuffer;
using ::media::DecryptConfig;
using ::media::DemuxerStream;
using ::media::MockDemuxerStream;
using ::media::MockRendererClient;
using ::testing::_;
using ::testing::DoAll;
using ::testing::MockFunction;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SaveArgByMove;

// Some default data. The values themselves are not relevant to the logic of the
// DemuxerStreamReader.
constexpr auto kBufferData = std::to_array<uint8_t>({1, 2, 3, 4, 5});
constexpr std::string_view kIv = "0123456789ABCDEF";
static_assert(kIv.size() == DecryptConfig::kDecryptionKeySize);
constexpr std::string_view kIdentifier = "drm_key_1";
constexpr StarboardDrmSubSampleMapping kSubsampleMapping = {
    .clear_byte_count = 0,
    .encrypted_byte_count = kBufferData.size(),
};

// Matches a DecoderBuffer.
MATCHER_P(DecoderBufferMatches, expected_buffer, "") {
  const DecoderBuffer& decoder_buffer = arg;
  return decoder_buffer.MatchesForTesting(expected_buffer);
}

// Creates an audio sample containing the given data and (optional) DRM info.
StarboardSampleInfo CreateAudioSample(
    base::span<const uint8_t> data,
    StarboardDrmSampleInfo* drm_info = nullptr) {
  StarboardSampleInfo sample_info;
  sample_info.type = 0;  // Audio
  sample_info.buffer = data.data();
  sample_info.buffer_size = data.size();
  sample_info.timestamp = 0;
  sample_info.side_data = base::span<const StarboardSampleSideData>();
  sample_info.audio_sample_info = {};
  sample_info.audio_sample_info.codec =
      StarboardAudioCodec::kStarboardAudioCodecAac;
  sample_info.audio_sample_info.mime = "";
  sample_info.drm_info = drm_info;

  return sample_info;
}

// Creates a video sample containing the given data and (optional) DRM info.
StarboardSampleInfo CreateVideoSample(
    base::span<const uint8_t> data,
    StarboardDrmSampleInfo* drm_info = nullptr) {
  StarboardSampleInfo sample_info;
  sample_info.type = 1;  // Video
  sample_info.buffer = data.data();
  sample_info.buffer_size = data.size();
  sample_info.timestamp = 0;
  sample_info.side_data = base::span<const StarboardSampleSideData>();
  sample_info.video_sample_info.codec =
      StarboardVideoCodec::kStarboardVideoCodecH265;
  sample_info.video_sample_info.mime = "";
  sample_info.video_sample_info.max_video_capabilities = "";
  sample_info.video_sample_info.is_key_frame = false;
  sample_info.video_sample_info.frame_width = 1920;
  sample_info.video_sample_info.frame_height = 1080;
  sample_info.video_sample_info.color_metadata = {};
  sample_info.drm_info = drm_info;

  return sample_info;
}

// A test fixture is used to avoid a bit of boilerplate in each test (creating
// the task environment, mocks, etc).
class DemuxerStreamReaderTest : public ::testing::Test {
 protected:
  DemuxerStreamReaderTest()
      : audio_stream_(DemuxerStream::Type::AUDIO),
        video_stream_(DemuxerStream::Type::VIDEO) {
    // Ensure that tests begin with a clean slate regarding DRM keys.
    StarboardDrmKeyTracker::GetInstance().ClearStateForTesting();
  }

  ~DemuxerStreamReaderTest() override = default;

  // This should be destructed last.
  SingleThreadTaskEnvironment task_environment_;
  MockDemuxerStream audio_stream_;
  MockDemuxerStream video_stream_;
  MockRendererClient renderer_client_;
  MockFunction<void(int seek_ticket,
                    StarboardSampleInfo sample_info,
                    scoped_refptr<::media::DecoderBuffer> buffer)>
      handle_buffer_cb_;
  MockFunction<void(int seek_ticket, StarboardMediaType type)> handle_eos_cb_;
};

TEST_F(DemuxerStreamReaderTest, ReadsVideoBufferAndCallsBufferCb) {
  constexpr int kSeekTicket = 7;
  scoped_refptr<DecoderBuffer> buffer =
      DecoderBuffer::CopyFrom({1, 2, 3, 4, 5});
  StarboardSampleInfo expected_info = CreateVideoSample(*buffer);

  EXPECT_CALL(handle_eos_cb_, Call).Times(0);
  EXPECT_CALL(handle_buffer_cb_,
              Call(kSeekTicket, MatchesStarboardSampleInfo(expected_info),
                   Pointee(DecoderBufferMatches(std::cref(*buffer)))))
      .Times(1);
  base::OnceCallback<void(
      DemuxerStream::Status status,
      std::vector<scoped_refptr<::media::DecoderBuffer>> buffers)>
      read_cb;
  EXPECT_CALL(video_stream_, OnRead).WillOnce(SaveArgByMove<0>(&read_cb));

  DemuxerStreamReader stream_reader(
      /*audio_stream=*/nullptr, &video_stream_,
      /*audio_sample_info=*/std::nullopt, expected_info.video_sample_info,
      base::BindLambdaForTesting(handle_buffer_cb_.AsStdFunction()),
      base::BindLambdaForTesting(handle_eos_cb_.AsStdFunction()),
      &renderer_client_);
  stream_reader.ReadBuffer(kSeekTicket,
                           StarboardMediaType::kStarboardMediaTypeVideo);

  // Simulate the DemuxerStream providing a buffer.
  ASSERT_FALSE(read_cb.is_null());
  std::move(read_cb).Run(DemuxerStream::Status::kOk, {buffer});
}

TEST_F(DemuxerStreamReaderTest, ReadsVideoBufferAndCallsEosCb) {
  constexpr int kSeekTicket = 7;
  StarboardVideoSampleInfo video_sample_info = {};

  EXPECT_CALL(handle_eos_cb_,
              Call(kSeekTicket, StarboardMediaType::kStarboardMediaTypeVideo))
      .Times(1);
  EXPECT_CALL(handle_buffer_cb_, Call).Times(0);
  base::OnceCallback<void(
      DemuxerStream::Status status,
      std::vector<scoped_refptr<::media::DecoderBuffer>> buffers)>
      read_cb;
  EXPECT_CALL(video_stream_, OnRead).WillOnce(SaveArgByMove<0>(&read_cb));

  DemuxerStreamReader stream_reader(
      /*audio_stream=*/nullptr, &video_stream_,
      /*audio_sample_info=*/std::nullopt, video_sample_info,
      base::BindLambdaForTesting(handle_buffer_cb_.AsStdFunction()),
      base::BindLambdaForTesting(handle_eos_cb_.AsStdFunction()),
      &renderer_client_);
  stream_reader.ReadBuffer(kSeekTicket,
                           StarboardMediaType::kStarboardMediaTypeVideo);

  // Simulate the DemuxerStream providing a buffer.
  ASSERT_FALSE(read_cb.is_null());
  std::move(read_cb).Run(DemuxerStream::Status::kOk,
                         {DecoderBuffer::CreateEOSBuffer()});
}

TEST_F(DemuxerStreamReaderTest, ReadsAudioBufferAndCallsBufferCb) {
  constexpr int kSeekTicket = 7;
  scoped_refptr<DecoderBuffer> buffer =
      DecoderBuffer::CopyFrom({1, 2, 3, 4, 5, 6, 7, 8});
  StarboardSampleInfo expected_info = CreateAudioSample(*buffer);

  EXPECT_CALL(handle_eos_cb_, Call).Times(0);
  EXPECT_CALL(handle_buffer_cb_,
              Call(kSeekTicket, MatchesStarboardSampleInfo(expected_info),
                   Pointee(DecoderBufferMatches(std::cref(*buffer)))))
      .Times(1);
  base::OnceCallback<void(
      DemuxerStream::Status status,
      std::vector<scoped_refptr<::media::DecoderBuffer>> buffers)>
      read_cb;
  EXPECT_CALL(audio_stream_, OnRead).WillOnce(SaveArgByMove<0>(&read_cb));

  DemuxerStreamReader stream_reader(
      &audio_stream_, /*video_stream=*/nullptr, expected_info.audio_sample_info,
      /*video_sample_info=*/std::nullopt,
      base::BindLambdaForTesting(handle_buffer_cb_.AsStdFunction()),
      base::BindLambdaForTesting(handle_eos_cb_.AsStdFunction()),
      &renderer_client_);
  stream_reader.ReadBuffer(kSeekTicket,
                           StarboardMediaType::kStarboardMediaTypeAudio);

  // Simulate the DemuxerStream providing a buffer.
  ASSERT_FALSE(read_cb.is_null());
  std::move(read_cb).Run(DemuxerStream::Status::kOk, {buffer});
}

TEST_F(DemuxerStreamReaderTest, ReadsAudioBufferAndCallsEosCb) {
  constexpr int kSeekTicket = 7;
  StarboardAudioSampleInfo audio_sample_info = {};

  EXPECT_CALL(handle_eos_cb_,
              Call(kSeekTicket, StarboardMediaType::kStarboardMediaTypeAudio))
      .Times(1);
  EXPECT_CALL(handle_buffer_cb_, Call).Times(0);
  base::OnceCallback<void(
      DemuxerStream::Status status,
      std::vector<scoped_refptr<::media::DecoderBuffer>> buffers)>
      read_cb;
  EXPECT_CALL(audio_stream_, OnRead).WillOnce(SaveArgByMove<0>(&read_cb));

  DemuxerStreamReader stream_reader(
      &audio_stream_, /*video_stream=*/nullptr, audio_sample_info,
      /*video_sample_info=*/std::nullopt,
      base::BindLambdaForTesting(handle_buffer_cb_.AsStdFunction()),
      base::BindLambdaForTesting(handle_eos_cb_.AsStdFunction()),
      &renderer_client_);
  stream_reader.ReadBuffer(kSeekTicket,
                           StarboardMediaType::kStarboardMediaTypeAudio);

  // Simulate the DemuxerStream providing a buffer.
  ASSERT_FALSE(read_cb.is_null());
  std::move(read_cb).Run(DemuxerStream::Status::kOk,
                         {DecoderBuffer::CreateEOSBuffer()});
}

TEST_F(DemuxerStreamReaderTest, ReadsAudioBufferAndConvertsPcmToS16) {
  constexpr int kSeekTicket = 7;
  // These values represent the min value, the max value, and 0. These can
  // easily be converted to S16 for comparison (see kS16Data below).
  constexpr auto kS32Data =
      std::to_array<uint32_t>({0x80000000, 0x7FFFFFFF, 0});
  // The equivalent to kS32Data, but represented as 16-bit samples.
  constexpr auto kS16Data = std::to_array<uint16_t>({0x8000, 0x7FFF, 0});
  scoped_refptr<DecoderBuffer> buffer =
      DecoderBuffer::CopyFrom(base::as_byte_span(kS32Data));
  scoped_refptr<DecoderBuffer> s16_buffer =
      DecoderBuffer::CopyFrom(base::as_byte_span(kS16Data));
  StarboardSampleInfo expected_info = CreateAudioSample(*buffer);
  expected_info.audio_sample_info.codec =
      StarboardAudioCodec::kStarboardAudioCodecPcm;

  // The relevant bits of information here are:
  // * codec = PCM (so that the DemuxerStreamReader converts it to S16)
  // * sample format = S32
  // * channel layout = mono (since there are only 3 samples in kS32Data)
  // * encryption scheme = unencrypted
  audio_stream_.set_audio_decoder_config(AudioDecoderConfig(
      ::media::AudioCodec::kPCM, ::media::SampleFormat::kSampleFormatS32,
      ::media::ChannelLayout::CHANNEL_LAYOUT_MONO, 44100, {},
      ::media::EncryptionScheme::kUnencrypted));

  EXPECT_CALL(handle_eos_cb_, Call).Times(0);
  scoped_refptr<DecoderBuffer> captured_buffer;
  StarboardSampleInfo captured_sample_info;
  EXPECT_CALL(handle_buffer_cb_,
              Call(kSeekTicket, _,
                   Pointee(DecoderBufferMatches(std::cref(*s16_buffer)))))
      .WillOnce(DoAll(SaveArg<1>(&captured_sample_info),
                      SaveArg<2>(&captured_buffer)));
  base::OnceCallback<void(
      DemuxerStream::Status status,
      std::vector<scoped_refptr<::media::DecoderBuffer>> buffers)>
      read_cb;
  EXPECT_CALL(audio_stream_, OnRead).WillOnce(SaveArgByMove<0>(&read_cb));

  DemuxerStreamReader stream_reader(
      &audio_stream_, /*video_stream=*/nullptr, expected_info.audio_sample_info,
      /*video_sample_info=*/std::nullopt,
      base::BindLambdaForTesting(handle_buffer_cb_.AsStdFunction()),
      base::BindLambdaForTesting(handle_eos_cb_.AsStdFunction()),
      &renderer_client_);
  stream_reader.ReadBuffer(kSeekTicket,
                           StarboardMediaType::kStarboardMediaTypeAudio);

  // Simulate the DemuxerStream providing a buffer.
  ASSERT_FALSE(read_cb.is_null());
  std::move(read_cb).Run(DemuxerStream::Status::kOk, {buffer});

  ASSERT_THAT(captured_buffer, NotNull());

  // Update the expectations to match the buffer that was returned from the
  // DemuxerStreamReader. That info needs to be match the info in the starboard
  // structs, so that the data passed to starboard has its lifetime managed
  // properly.
  expected_info.buffer = captured_buffer->data();
  expected_info.buffer_size = captured_buffer->size();
  EXPECT_THAT(captured_sample_info, MatchesStarboardSampleInfo(expected_info));
}

TEST_F(DemuxerStreamReaderTest,
       ReadsVideoBufferButDoesNotCallCbIfDrmKeyUnavailable) {
  constexpr int kSeekTicket = 7;
  scoped_refptr<DecoderBuffer> buffer = DecoderBuffer::CopyFrom(kBufferData);
  buffer->set_decrypt_config(DecryptConfig::CreateCencConfig(
      std::string(kIdentifier), std::string(kIv),
      /*subsamples=*/{}));

  StarboardDrmSampleInfo drm_info;
  drm_info.encryption_scheme =
      StarboardDrmEncryptionScheme::kStarboardDrmEncryptionSchemeAesCtr;
  // CENC encryption scheme does not use an encryption pattern.
  drm_info.encryption_pattern.crypt_byte_block = 0;
  drm_info.encryption_pattern.skip_byte_block = 0;
  base::span<uint8_t>(drm_info.initialization_vector)
      .first<kIv.size()>()
      .copy_from(base::as_byte_span(kIv));
  drm_info.initialization_vector_size = kIv.size();
  base::span<uint8_t>(drm_info.identifier)
      .first<kIdentifier.size()>()
      .copy_from(base::as_byte_span(kIdentifier));
  drm_info.identifier_size = kIdentifier.size();
  drm_info.subsample_mapping = base::span_from_ref(kSubsampleMapping);

  StarboardSampleInfo expected_info = CreateVideoSample(*buffer, &drm_info);

  EXPECT_CALL(handle_eos_cb_, Call).Times(0);
  // This should not be called, since the DRM key is not available.
  EXPECT_CALL(handle_buffer_cb_, Call).Times(0);
  base::OnceCallback<void(
      DemuxerStream::Status status,
      std::vector<scoped_refptr<::media::DecoderBuffer>> buffers)>
      read_cb;
  EXPECT_CALL(video_stream_, OnRead).WillOnce(SaveArgByMove<0>(&read_cb));

  DemuxerStreamReader stream_reader(
      /*audio_stream=*/nullptr, &video_stream_,
      /*audio_sample_info=*/std::nullopt, expected_info.video_sample_info,
      base::BindLambdaForTesting(handle_buffer_cb_.AsStdFunction()),
      base::BindLambdaForTesting(handle_eos_cb_.AsStdFunction()),
      &renderer_client_);
  stream_reader.ReadBuffer(kSeekTicket,
                           StarboardMediaType::kStarboardMediaTypeVideo);

  // Simulate the DemuxerStream providing a buffer.
  ASSERT_FALSE(read_cb.is_null());
  std::move(read_cb).Run(DemuxerStream::Status::kOk, {buffer});
}

TEST_F(DemuxerStreamReaderTest,
       ReadsVideoBufferAndCallsCbWhenDrmKeyIsAvailable) {
  constexpr int kSeekTicket = 7;
  scoped_refptr<DecoderBuffer> buffer = DecoderBuffer::CopyFrom(kBufferData);
  buffer->set_decrypt_config(DecryptConfig::CreateCencConfig(
      std::string(kIdentifier), std::string(kIv),
      /*subsamples=*/{}));

  StarboardDrmSampleInfo drm_info;
  drm_info.encryption_scheme =
      StarboardDrmEncryptionScheme::kStarboardDrmEncryptionSchemeAesCtr;
  // CENC encryption scheme does not use an encryption pattern.
  drm_info.encryption_pattern.crypt_byte_block = 0;
  drm_info.encryption_pattern.skip_byte_block = 0;
  base::span<uint8_t>(drm_info.initialization_vector)
      .first<kIv.size()>()
      .copy_from(base::as_byte_span(kIv));
  drm_info.initialization_vector_size = kIv.size();
  base::span<uint8_t>(drm_info.identifier)
      .first<kIdentifier.size()>()
      .copy_from(base::as_byte_span(kIdentifier));
  drm_info.identifier_size = kIdentifier.size();
  drm_info.subsample_mapping = base::span_from_ref(kSubsampleMapping);

  StarboardSampleInfo expected_info = CreateVideoSample(*buffer, &drm_info);

  EXPECT_CALL(handle_eos_cb_, Call).Times(0);
  // This should be called once the DRM key is available.
  EXPECT_CALL(handle_buffer_cb_,
              Call(kSeekTicket, MatchesStarboardSampleInfo(expected_info),
                   Pointee(DecoderBufferMatches(std::cref(*buffer)))))
      .Times(1);
  base::OnceCallback<void(
      DemuxerStream::Status status,
      std::vector<scoped_refptr<::media::DecoderBuffer>> buffers)>
      read_cb;
  EXPECT_CALL(video_stream_, OnRead).WillOnce(SaveArgByMove<0>(&read_cb));

  DemuxerStreamReader stream_reader(
      /*audio_stream=*/nullptr, &video_stream_,
      /*audio_sample_info=*/std::nullopt, expected_info.video_sample_info,
      base::BindLambdaForTesting(handle_buffer_cb_.AsStdFunction()),
      base::BindLambdaForTesting(handle_eos_cb_.AsStdFunction()),
      &renderer_client_);
  stream_reader.ReadBuffer(kSeekTicket,
                           StarboardMediaType::kStarboardMediaTypeVideo);

  // Simulate the DemuxerStream providing a buffer.
  ASSERT_FALSE(read_cb.is_null());
  std::move(read_cb).Run(DemuxerStream::Status::kOk, {buffer});

  // Simulate the DRM key being available. This should trigger the call to
  // handle_buffer_cb_.
  StarboardDrmKeyTracker::GetInstance().AddKey(std::string(kIdentifier),
                                               "some_session");

  // Use a run loop here, since the DRM key callback may be posted to a separate
  // task.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace
}  // namespace media
}  // namespace chromecast
