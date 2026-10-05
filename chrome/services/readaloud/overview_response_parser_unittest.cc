// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/overview_response_parser.h"

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "components/optimization_guide/proto/features/read_aloud_generate_text.pb.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

namespace {

using optimization_guide::proto::ReadAloudGenerateTextResponse;
using read_aloud::mojom::Speaker;

void AddTurn(ReadAloudGenerateTextResponse& response,
             std::string_view speaker,
             std::string_view utterance) {
  optimization_guide::proto::DialogueTurn* turn =
      response.add_dialogue_turns();
  if (!speaker.empty()) {
    turn->set_speaker(speaker);
  }
  turn->set_utterance(utterance);
}

ParsedOverviewResult Parse(const ReadAloudGenerateTextResponse& response) {
  std::string serialized = response.SerializeAsString();
  mojo_base::BigBuffer buffer(base::as_byte_span(serialized));
  return ParseAndValidateOverviewResponse(std::move(buffer));
}

MATCHER_P3(IsSegment, index, text, speaker, "") {
  return arg->segment_index == index && arg->text == text &&
         arg->speaker == speaker;
}

}  // namespace

TEST(OverviewResponseParserTest, DialogueTurnsBecomeSegments) {
  // Build a response with a title and dialogue turns.
  ReadAloudGenerateTextResponse response;
  response.set_title("Title");
  AddTurn(response, "Host", "  Hello.  ");
  AddTurn(response, "", "");         // Empty turn should be skipped.
  AddTurn(response, "Host", "\n\t");  // Whitespace-only turn should be skipped.
  AddTurn(response, "Reporter", "Hi.");

  ParsedOverviewResult result = Parse(response);
  // Verify status is OK and title was extracted.
  EXPECT_EQ(result.status, OverviewParseStatus::kOk);
  EXPECT_EQ(result.title, "Title");

  // Verify non-empty turns became sequential segments with correct speakers
  // and trimmed text.
  EXPECT_THAT(result.segments,
              testing::ElementsAre(
                  IsSegment(0u, u"Hello.", Speaker::kSpeaker1),
                  IsSegment(1u, u"Hi.", Speaker::kSpeaker2)));
}

TEST(OverviewResponseParserTest, SpeakerAssignedByRole) {
  // Add dialogue turns with various persona role names.
  ReadAloudGenerateTextResponse response;
  const char* kRoles[] = {"", "Host", "host", "Reporter", "Host", "Guest"};
  for (const char* role : kRoles) {
    AddTurn(response, role, "Text.");
  }

  ParsedOverviewResult result = Parse(response);
  ASSERT_EQ(result.status, OverviewParseStatus::kOk);
  ASSERT_EQ(result.segments.size(), 6u);
  // "Host" (case-insensitive) or unset role maps to Speaker 1.
  EXPECT_EQ(result.segments[0]->speaker, Speaker::kSpeaker1);
  EXPECT_EQ(result.segments[1]->speaker, Speaker::kSpeaker1);
  EXPECT_EQ(result.segments[2]->speaker, Speaker::kSpeaker1);
  // Any other role (e.g. "Reporter", "Guest") maps to Speaker 2.
  EXPECT_EQ(result.segments[3]->speaker, Speaker::kSpeaker2);
  EXPECT_EQ(result.segments[4]->speaker, Speaker::kSpeaker1);
  EXPECT_EQ(result.segments[5]->speaker, Speaker::kSpeaker2);
}

TEST(OverviewResponseParserTest, EmptyScriptYieldsEmptyStatus) {
  // Response with only a title and no turns yields kEmpty.
  ReadAloudGenerateTextResponse response;
  response.set_title("Overview Title");
  ParsedOverviewResult result = Parse(response);
  EXPECT_EQ(result.status, OverviewParseStatus::kEmpty);
  EXPECT_TRUE(result.title.empty());

  // Turns with empty utterances are discarded; if none remain, status is
  // kEmpty.
  AddTurn(response, "Host", "");
  result = Parse(response);
  EXPECT_EQ(result.status, OverviewParseStatus::kEmpty);
  EXPECT_TRUE(result.title.empty());
}

TEST(OverviewResponseParserTest, InvalidBufferIsMalformed) {
  // Empty buffer rejects as malformed.
  EXPECT_EQ(ParseAndValidateOverviewResponse(mojo_base::BigBuffer()).status,
            OverviewParseStatus::kMalformed);

  // Invalid binary data rejects as malformed.
  std::string invalid = "\xff\xff\xff";
  EXPECT_EQ(ParseAndValidateOverviewResponse(
                mojo_base::BigBuffer(base::as_byte_span(invalid)))
                .status,
            OverviewParseStatus::kMalformed);
}

TEST(OverviewResponseParserTest, TooManyTurnsIsMalformed) {
  // Build a response exceeding the maximum segment limit (kMaxTextSegments).
  ReadAloudGenerateTextResponse response;
  for (size_t i = 0; i <= kMaxTextSegments; ++i) {
    AddTurn(response, "", "a");
  }

  // Reject oversized turn count as malformed to prevent memory exhaustion.
  ParsedOverviewResult result = Parse(response);
  EXPECT_EQ(result.status, OverviewParseStatus::kMalformed);
  EXPECT_TRUE(result.segments.empty());
}

TEST(OverviewResponseParserTest, OversizedMetadataIsTruncated) {
  // Title exceeds the metadata limit.
  ReadAloudGenerateTextResponse response;
  response.set_title(std::string(kMaxOverviewMetadataLength + 1, 'a'));
  AddTurn(response, "Host", "Hello.");

  // Parser succeeds and safely truncates the oversized title to kMaxOverviewMetadataLength.
  ParsedOverviewResult result = Parse(response);
  EXPECT_EQ(result.status, OverviewParseStatus::kOk);
  EXPECT_EQ(result.title, std::string(kMaxOverviewMetadataLength, 'a'));
}

TEST(OverviewResponseParserTest, InvalidUtf8MetadataIsMalformed) {
  // Title contains invalid UTF-8 bytes.
  ReadAloudGenerateTextResponse response;
  response.set_title("Title\xff");
  AddTurn(response, "Host", "Hello.");

  // Protobuf deserialization rejects invalid UTF-8 string fields as malformed.
  ParsedOverviewResult result = Parse(response);
  EXPECT_EQ(result.status, OverviewParseStatus::kMalformed);
}

TEST(OverviewResponseParserTest, InvalidUtf8UtteranceIsMalformed) {
  // Utterance containing invalid UTF-8 bytes.
  ReadAloudGenerateTextResponse response;
  AddTurn(response, "", "a\xff");

  // Protobuf deserialization rejects invalid UTF-8 string fields as malformed.
  ParsedOverviewResult result = Parse(response);
  EXPECT_EQ(result.status, OverviewParseStatus::kMalformed);
}

TEST(OverviewResponseParserTest, InputLargerThan64KiBIsParsed) {
  // Crosses mojo_base::BigBuffer's 64 KiB inline threshold into shared memory.
  ReadAloudGenerateTextResponse response;
  for (int i = 0; i < 3; ++i) {
    AddTurn(response, "", std::string(30000, 'a'));
  }
  ASSERT_GT(response.ByteSizeLong(), 64u * 1024u);

  // Verifies shared-memory backed BigBuffer parses successfully.
  ParsedOverviewResult result = Parse(response);
  EXPECT_EQ(result.status, OverviewParseStatus::kOk);
  EXPECT_EQ(result.segments.size(), 3u);
}

}  // namespace readaloud
