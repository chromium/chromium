// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/decoder/opus_decoder_helper.h"

#include <vector>

#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/media_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

MATCHER_P(MatchesWordTiming, expected, "") {
  return arg.text == expected.text && arg.start_time == expected.start_time &&
         arg.end_time == expected.end_time;
}

class OpusDecoderHelperTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

#if BUILDFLAG(ENABLE_FFMPEG)

TEST_F(OpusDecoderHelperTest, DecodeReturnsEmptyListForEmptyBuffer) {
  OpusDecoderHelper helper;
  scoped_refptr<media::DecoderBuffer> empty_buffer =
      base::MakeRefCounted<media::DecoderBuffer>(0u);
  base::RunLoop run_loop;

  helper.DecodeAndSlice(
      empty_buffer, {}, {},
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::vector<scoped_refptr<DecodedAudioSegment>> segments) {
            EXPECT_TRUE(segments.empty());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(OpusDecoderHelperTest, DecodeInvalidAudioDataReturnsEmptyList) {
  std::vector<uint8_t> invalid_bytes = {0x00, 0x01, 0x02, 0x03, 0x04};
  scoped_refptr<media::DecoderBuffer> invalid_buffer =
      media::DecoderBuffer::CopyFrom(invalid_bytes);
  ASSERT_NE(nullptr, invalid_buffer);

  OpusDecoderHelper helper;
  base::RunLoop run_loop;

  helper.DecodeAndSlice(
      invalid_buffer, {}, {},
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::vector<scoped_refptr<DecodedAudioSegment>> segments) {
            EXPECT_TRUE(segments.empty());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(OpusDecoderHelperTest,
       DecodeTruncatedStreamWithoutFramesReturnsEmptyList) {
  base::FilePath file_path = media::GetTestDataFilePath("sfx-opus.ogg");
  base::MemoryMappedFile file;
  ASSERT_TRUE(file.Initialize(file_path));

  // Truncating the stream causes the reader to fail gracefully and return an
  // empty list of decoded segments.
  ASSERT_GT(file.length(), 200u);
  auto truncated_buffer =
      media::DecoderBuffer::CopyFrom(file.bytes().first(200u));
  ASSERT_NE(nullptr, truncated_buffer);

  OpusDecoderHelper helper;
  base::RunLoop run_loop;

  helper.DecodeAndSlice(
      truncated_buffer, {}, {},
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::vector<scoped_refptr<DecodedAudioSegment>> segments) {
            EXPECT_TRUE(segments.empty());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(OpusDecoderHelperTest, DecodeValidOggOpusStream) {
  base::FilePath file_path = media::GetTestDataFilePath("sfx-opus.ogg");
  base::MemoryMappedFile file;
  ASSERT_TRUE(file.Initialize(file_path));

  auto container_buffer = media::DecoderBuffer::CopyFrom(file.bytes());
  ASSERT_NE(nullptr, container_buffer);

  OpusDecoderHelper helper;
  std::vector<DecodedAudioSegment::WordTiming> timings = {
      {"Hello", base::Milliseconds(0), base::Milliseconds(200)},
      {"World", base::Milliseconds(200), base::Milliseconds(500)}};
  base::RunLoop run_loop;

  helper.DecodeAndSlice(
      container_buffer, timings, {},
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             const std::vector<DecodedAudioSegment::WordTiming>&
                 expected_timings,
             std::vector<scoped_refptr<DecodedAudioSegment>> segments) {
            ASSERT_EQ(1u, segments.size());
            EXPECT_GT(segments[0]->audio_buffer()->frame_count(), 0);
            EXPECT_THAT(
                segments[0]->word_timings(),
                testing::ElementsAre(MatchesWordTiming(expected_timings[0]),
                                     MatchesWordTiming(expected_timings[1])));
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), timings));

  run_loop.Run();
}

#else

TEST_F(OpusDecoderHelperTest, DecodeWithoutFFmpegReturnsEmptyList) {
  std::vector<uint8_t> bytes = {0x00, 0x01, 0x02, 0x03};
  scoped_refptr<media::DecoderBuffer> buffer =
      media::DecoderBuffer::CopyFrom(bytes);
  ASSERT_NE(nullptr, buffer);

  OpusDecoderHelper helper;
  base::RunLoop run_loop;

  helper.DecodeAndSlice(
      buffer, {}, {},
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::vector<scoped_refptr<DecodedAudioSegment>> segments) {
            EXPECT_TRUE(segments.empty());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  run_loop.Run();
}

#endif  // BUILDFLAG(ENABLE_FFMPEG)

}  // namespace readaloud
