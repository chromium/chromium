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

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::ResultOf;
MATCHER_P(MatchesWordTiming, expected, "") {
  return arg.text == expected.text && arg.start_time == expected.start_time &&
         arg.end_time == expected.end_time;
}

MATCHER_P2(MatchesSegment, duration_matcher, timings_matcher, "") {
  return ExplainMatchResult(
      Pointee(AllOf(
          Property(&DecodedAudioSegment::audio_buffer,
                   AllOf(NotNull(),
                         Pointee(AllOf(
                             Property(&media::AudioBuffer::frame_count, Gt(0)),
                             Property(&media::AudioBuffer::duration,
                                      duration_matcher))))),
          Property(&DecodedAudioSegment::word_timings, timings_matcher))),
      arg, result_listener);
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
      empty_buffer, {},
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
      invalid_buffer, {},
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
      truncated_buffer, {},
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

  scoped_refptr<media::DecoderBuffer> container_buffer =
      media::DecoderBuffer::CopyFrom(file.bytes());
  ASSERT_NE(container_buffer, nullptr);

  OpusDecoderHelper helper;
  std::vector<DecodedAudioSegment::WordTiming> timings = {
      {"Hello", base::Milliseconds(0), base::Milliseconds(120)},
      {"World", base::Milliseconds(120), base::Milliseconds(270)}};
  base::RunLoop run_loop;

  helper.DecodeAndSlice(
      container_buffer, timings,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             const std::vector<DecodedAudioSegment::WordTiming>&
                 expected_timings,
             std::vector<scoped_refptr<DecodedAudioSegment>> segments) {
            ASSERT_EQ(segments.size(), expected_timings.size());
            DecodedAudioSegment::WordTiming expected_timing0 = {
                "Hello", base::Milliseconds(0), base::Milliseconds(120)};
            DecodedAudioSegment::WordTiming expected_timing1 = {
                "World", base::Milliseconds(0), base::Milliseconds(150)};
            EXPECT_THAT(
                segments,
                ElementsAre(
                    MatchesSegment(
                        /*duration_matcher=*/Eq(base::Milliseconds(120)),
                        /*timings_matcher=*/ElementsAre(
                            MatchesWordTiming(expected_timing0))),
                    MatchesSegment(
                        /*duration_matcher=*/Eq(base::Milliseconds(150)),
                        /*timings_matcher=*/ElementsAre(
                            MatchesWordTiming(expected_timing1)))));

            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), timings));

  run_loop.Run();
}

TEST_F(OpusDecoderHelperTest, DecodeHandlesOutOfBoundsWordTimings) {
  base::FilePath file_path = media::GetTestDataFilePath("sfx-opus.ogg");
  base::MemoryMappedFile file;
  ASSERT_TRUE(file.Initialize(file_path));

  scoped_refptr<media::DecoderBuffer> container_buffer =
      media::DecoderBuffer::CopyFrom(file.bytes());
  ASSERT_NE(container_buffer, nullptr);

  OpusDecoderHelper helper;
  std::vector<DecodedAudioSegment::WordTiming> timings = {
      {"Valid", base::Milliseconds(0), base::Milliseconds(50)},
      {"StartsValidEndsPastEnd", base::Milliseconds(50),
       base::Milliseconds(50000)},
      {"Out", base::Milliseconds(50000), base::Milliseconds(60000)}};
  base::RunLoop run_loop;

  helper.DecodeAndSlice(
      container_buffer, timings,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::vector<scoped_refptr<DecodedAudioSegment>> segments) {
            ASSERT_EQ(segments.size(), 2u);
            EXPECT_THAT(
                segments,
                ElementsAre(MatchesSegment(
                                /*duration_matcher=*/Eq(base::Milliseconds(50)),
                                /*timings_matcher=*/testing::_),
                            MatchesSegment(
                                /*duration_matcher=*/Gt(base::Milliseconds(0)),
                                /*timings_matcher=*/testing::_)));
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(OpusDecoderHelperTest, DecodeWithEmptyTimingsReturnsFullBuffer) {
  base::FilePath file_path = media::GetTestDataFilePath("sfx-opus.ogg");
  base::MemoryMappedFile file;
  ASSERT_TRUE(file.Initialize(file_path));

  scoped_refptr<media::DecoderBuffer> container_buffer =
      media::DecoderBuffer::CopyFrom(file.bytes());
  ASSERT_NE(container_buffer, nullptr);

  OpusDecoderHelper helper;
  base::RunLoop run_loop;

  helper.DecodeAndSlice(
      container_buffer, /*timings=*/{},
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::vector<scoped_refptr<DecodedAudioSegment>> segments) {
            ASSERT_EQ(segments.size(), 1u);
            ASSERT_TRUE(segments[0]->audio_buffer());
            EXPECT_GT(segments[0]->audio_buffer()->frame_count(), 0);
            EXPECT_TRUE(segments[0]->word_timings().empty());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

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
      buffer, {},
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
