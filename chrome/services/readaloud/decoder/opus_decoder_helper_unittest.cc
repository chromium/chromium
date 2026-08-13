// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/decoder/opus_decoder_helper.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "media/base/decoder_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

class OpusDecoderHelperTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

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

TEST_F(OpusDecoderHelperTest, DecodeReturnsDummyForValidBuffer) {
  OpusDecoderHelper helper;
  std::vector<uint8_t> valid_bytes = {0x4F, 0x67, 0x67, 0x53};  // "OggS"
  scoped_refptr<media::DecoderBuffer> valid_buffer =
      media::DecoderBuffer::CopyFrom(valid_bytes);
  std::vector<DecodedAudioSegment::WordTiming> timings = {
      {"Hello", base::Milliseconds(0), base::Milliseconds(200)},
      {"World", base::Milliseconds(200), base::Milliseconds(500)}};
  base::RunLoop run_loop;

  helper.DecodeAndSlice(
      valid_buffer, timings, {},
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             const std::vector<DecodedAudioSegment::WordTiming>&
                 expected_timings,
             std::vector<scoped_refptr<DecodedAudioSegment>> segments) {
            ASSERT_EQ(1u, segments.size());
            ASSERT_NE(nullptr, segments[0]);
            EXPECT_NE(nullptr, segments[0]->audio_buffer());
            EXPECT_EQ(expected_timings.size(),
                      segments[0]->word_timings().size());
            EXPECT_EQ("Hello", segments[0]->word_timings()[0].text);
            EXPECT_EQ(base::Milliseconds(0),
                      segments[0]->word_timings()[0].start_time);
            EXPECT_EQ(base::Milliseconds(200),
                      segments[0]->word_timings()[0].end_time);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), timings));

  run_loop.Run();
}

}  // namespace readaloud
