// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/synthesis_response_parser.h"

#include <string>
#include <string_view>
#include <utility>

#include "chrome/services/readaloud/chunking/text_chunker.h"
#include "chrome/services/readaloud/word_timing.h"
#include "components/optimization_guide/proto/features/read_aloud_synthesize.pb.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

TEST(SynthesisResponseParserTest, CalculateMonotonicTimingBoundsValid) {
  TimingBounds bounds = CalculateMonotonicTimingBounds(/*raw_start_ms=*/100, /*raw_next_start_ms=*/250);
  EXPECT_EQ(bounds.start_time, base::Milliseconds(100));
  EXPECT_EQ(bounds.end_time, base::Milliseconds(250));
}

TEST(SynthesisResponseParserTest, CalculateMonotonicTimingBoundsNegativeClamping) {
  TimingBounds bounds = CalculateMonotonicTimingBounds(/*raw_start_ms=*/-500, /*raw_next_start_ms=*/-100);
  EXPECT_EQ(bounds.start_time, base::Milliseconds(0));
  EXPECT_GT(bounds.end_time, bounds.start_time);
}

TEST(SynthesisResponseParserTest, CalculateMonotonicTimingBoundsNonMonotonicFallback) {
  TimingBounds bounds = CalculateMonotonicTimingBounds(/*raw_start_ms=*/300, /*raw_next_start_ms=*/150);
  EXPECT_EQ(bounds.start_time, base::Milliseconds(300));
  EXPECT_GT(bounds.end_time, bounds.start_time);
}

TEST(SynthesisResponseParserTest, ParseWordTimingsNegativeOffsetsClamped) {
  optimization_guide::proto::ReadAloudSynthesizeResponse response;
  response.set_audio_bytes("valid_bytes");
  auto* timing = response.add_timings();
  timing->set_start_offset(-10);
  timing->set_end_offset(-5);
  timing->set_time_offset_ms(0);

  std::string serialized;
  ASSERT_TRUE(response.SerializeToString(&serialized));
  mojo_base::BigBuffer buffer(base::as_byte_span(serialized));

  TextChunk chunk{u"Hello world", /*start_code_unit_offset=*/0};
  ParsedSynthesisResult result =
      ParseAndValidateSynthesisResponse(std::move(buffer), chunk);
  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.timings.size(), 1u);
  EXPECT_EQ(result.timings[0].start_character_offset, 0u);
  EXPECT_EQ(result.timings[0].end_character_offset, 0u);
}

TEST(SynthesisResponseParserTest, ParseWordTimingsInvertedOffsetsNormalized) {
  optimization_guide::proto::ReadAloudSynthesizeResponse response;
  response.set_audio_bytes("valid_bytes");
  auto* timing = response.add_timings();
  timing->set_start_offset(100);
  timing->set_end_offset(20);  // Inverted: end < start
  timing->set_time_offset_ms(0);

  std::string serialized;
  ASSERT_TRUE(response.SerializeToString(&serialized));
  mojo_base::BigBuffer buffer(base::as_byte_span(serialized));

  std::u16string long_text(120, u'a');
  TextChunk chunk{long_text, /*start_code_unit_offset=*/0};
  ParsedSynthesisResult result =
      ParseAndValidateSynthesisResponse(std::move(buffer), chunk);
  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.timings.size(), 1u);
  EXPECT_EQ(result.timings[0].start_character_offset, 100u);
  EXPECT_EQ(result.timings[0].end_character_offset, 100u);
}

TEST(SynthesisResponseParserTest, ParseWordTimingsBaseOffsetApplied) {
  optimization_guide::proto::ReadAloudSynthesizeResponse response;
  response.set_audio_bytes("valid_bytes");
  auto* timing = response.add_timings();
  timing->set_start_offset(0);
  timing->set_end_offset(5);
  timing->set_time_offset_ms(0);

  std::string serialized;
  ASSERT_TRUE(response.SerializeToString(&serialized));
  mojo_base::BigBuffer buffer(base::as_byte_span(serialized));

  TextChunk chunk{u"Hello world", /*start_code_unit_offset=*/100};
  ParsedSynthesisResult result =
      ParseAndValidateSynthesisResponse(std::move(buffer), chunk);
  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.timings.size(), 1u);
  EXPECT_EQ(result.timings[0].start_character_offset, 100u);
  EXPECT_EQ(result.timings[0].end_character_offset, 105u);
}

TEST(SynthesisResponseParserTest, ParseWordTimingsExceedingChunkLengthClamped) {
  optimization_guide::proto::ReadAloudSynthesizeResponse response;
  response.set_audio_bytes("valid_bytes");
  auto* timing = response.add_timings();
  timing->set_start_offset(2);
  timing->set_end_offset(50);
  timing->set_time_offset_ms(0);

  std::string serialized;
  ASSERT_TRUE(response.SerializeToString(&serialized));
  mojo_base::BigBuffer buffer(base::as_byte_span(serialized));

  TextChunk chunk{u"Hello", /*start_code_unit_offset=*/10};
  ParsedSynthesisResult result =
      ParseAndValidateSynthesisResponse(std::move(buffer), chunk);
  ASSERT_TRUE(result.success);
  ASSERT_EQ(result.timings.size(), 1u);
  EXPECT_EQ(result.timings[0].start_character_offset, 12u);
  EXPECT_EQ(result.timings[0].end_character_offset, 15u);
}

TEST(SynthesisResponseParserTest, ParseValidProtobuf) {
  optimization_guide::proto::ReadAloudSynthesizeResponse response;
  response.set_audio_bytes("valid_opus_bytes");

  optimization_guide::proto::WordTiming* timing1 = response.add_timings();
  timing1->set_start_offset(0);
  timing1->set_end_offset(5);
  timing1->set_time_offset_ms(0);

  std::string serialized;
  ASSERT_TRUE(response.SerializeToString(&serialized));

  mojo_base::BigBuffer buffer(base::as_byte_span(serialized));
  TextChunk chunk{u"Hello world", /*start_code_unit_offset=*/0};
  ParsedSynthesisResult result =
      ParseAndValidateSynthesisResponse(std::move(buffer), chunk);

  EXPECT_TRUE(result.success);
  ASSERT_NE(result.audio_buffer, nullptr);
  EXPECT_EQ(result.audio_buffer->size(),
            std::string_view("valid_opus_bytes").size());
  ASSERT_EQ(result.timings.size(), 1u);
  EXPECT_EQ(result.timings[0].start_character_offset, 0u);
  EXPECT_EQ(result.timings[0].end_character_offset, 5u);
}

TEST(SynthesisResponseParserTest, ParseMalformedProtobuf) {
  std::string malformed = "not_a_valid_protobuf_payload";
  mojo_base::BigBuffer buffer(base::as_byte_span(malformed));

  TextChunk chunk{u"Hello world", /*start_code_unit_offset=*/0};
  ParsedSynthesisResult result =
      ParseAndValidateSynthesisResponse(std::move(buffer), chunk);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.audio_buffer, nullptr);
  EXPECT_TRUE(result.timings.empty());
}

}  // namespace readaloud
