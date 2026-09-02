// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/synthesis_response_parser.h"

#include <string>

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

TEST(SynthesisResponseParserTest, ExtractUTF8WordTextValid) {
  EXPECT_EQ(ExtractUTF8WordText(u"Hello world", 0, 5), "Hello");
  EXPECT_EQ(ExtractUTF8WordText(u"Hello world", 6, 11), "world");
}

TEST(SynthesisResponseParserTest, ExtractUTF8WordTextSurrogatePairAlignment) {
  // u"Hello 😀 world" where 😀 is a 2-code-unit surrogate pair at offsets 6-7.
  std::u16string emoji_text = u"Hello 😀 world";
  // Slicing mid-surrogate (offset 7) aligns left to include the full emoji.
  EXPECT_EQ(ExtractUTF8WordText(emoji_text, 6, 8), "😀");
}

TEST(SynthesisResponseParserTest, ExtractUTF8WordTextOutOfBounds) {
  EXPECT_EQ(ExtractUTF8WordText(u"Short", 10, 20), "");
  EXPECT_EQ(ExtractUTF8WordText(u"Short", -1, 3), "");
  EXPECT_EQ(ExtractUTF8WordText(u"Short", 3, 2), "");
  EXPECT_EQ(ExtractUTF8WordText(u"", 0, 1), "");
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
  ParsedSynthesisResult result =
      ParseAndValidateSynthesisResponse(std::move(buffer), u"Hello world");

  EXPECT_TRUE(result.success);
  ASSERT_NE(result.audio_buffer, nullptr);
  EXPECT_EQ(result.audio_buffer->size(), std::string("valid_opus_bytes").size());
  ASSERT_EQ(result.timings.size(), 1u);
  EXPECT_EQ(result.timings[0].text, "Hello");
}

TEST(SynthesisResponseParserTest, ParseMalformedProtobuf) {
  std::string malformed = "not_a_valid_protobuf_payload";
  mojo_base::BigBuffer buffer(base::as_byte_span(malformed));

  ParsedSynthesisResult result =
      ParseAndValidateSynthesisResponse(std::move(buffer), u"Hello world");

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.audio_buffer, nullptr);
  EXPECT_TRUE(result.timings.empty());
}

}  // namespace readaloud
