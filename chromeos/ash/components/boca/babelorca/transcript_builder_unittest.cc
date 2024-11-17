// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/transcript_builder.h"

#include <string>
#include <utility>
#include <vector>

#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

constexpr char kSessionId[] = "session id";
constexpr char kSenderEmail[] = "sender@mail.com";
constexpr char kLanguageEn[] = "en";

struct TranscriptPartOptions {
  int64_t id = 12;
  int64_t index = 15;
  std::string text = "default";
  bool is_final = false;
  std::string language = kLanguageEn;
};

struct MessageOptions {
  std::string session_id = kSessionId;
  std::string sender_email = kSenderEmail;
  int64_t init_timestamp_ms = 123456789;
  int64_t order = 1;
  TranscriptPartOptions previous_transcript_opts;
  TranscriptPartOptions current_transcript_opts;
};

mojom::TranscriptPartPtr CreateTranscriptPart(TranscriptPartOptions opts) {
  return mojom::TranscriptPart::New(opts.id, opts.index, opts.text,
                                    opts.is_final, opts.language);
}

mojom::BabelOrcaMessagePtr CreateMessage(
    MessageOptions opts,
    mojom::TranscriptPartPtr previous_transcript,
    mojom::TranscriptPartPtr current_transcript) {
  return mojom::BabelOrcaMessage::New(
      opts.sender_email, opts.session_id, opts.init_timestamp_ms, opts.order,
      std::move(previous_transcript), std::move(current_transcript));
}

void VerifyBuilderResult(TranscriptBuilder::Result lhs,
                         TranscriptBuilder::Result rhs) {
  EXPECT_EQ(lhs.text, rhs.text);
  EXPECT_EQ(lhs.is_final, rhs.is_final);
  EXPECT_EQ(lhs.language, rhs.language);
}

struct TranscriptBuilderDiscardTestCase {
  std::string test_name;
  MessageOptions opts;
};

using TranscriptBuilderDiscardTest =
    testing::TestWithParam<TranscriptBuilderDiscardTestCase>;

TEST_P(TranscriptBuilderDiscardTest, Invalid) {
  const std::string kCurrentText = "current text";
  TranscriptBuilder builder(kSessionId, kSenderEmail);
  mojom::BabelOrcaMessagePtr message = CreateMessage(
      GetParam().opts,
      /*previous_transcript=*/CreateTranscriptPart({}),
      /*current_transcript=*/CreateTranscriptPart({.text = kCurrentText}));

  std::vector<TranscriptBuilder::Result> results =
      builder.GetTranscripts(std::move(message));

  EXPECT_THAT(results, testing::IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(
    TranscriptBuilderDiscardTestSuite,
    TranscriptBuilderDiscardTest,
    testing::ValuesIn<TranscriptBuilderDiscardTestCase>({
        {"SenderEmail", {.sender_email = "wrong@email.com"}},
        {"SessionId", {.session_id = "wrong session id"}},
    }),
    [](const testing::TestParamInfo<TranscriptBuilderDiscardTest::ParamType>&
           info) { return info.param.test_name; });

struct TranscriptBuilderDiscardNextTestCase {
  std::string test_name;
  MessageOptions opts1;
  MessageOptions opts2;
};

using TranscriptBuilderDiscardNextTest =
    testing::TestWithParam<TranscriptBuilderDiscardNextTestCase>;

TEST_P(TranscriptBuilderDiscardNextTest, Discard) {
  const std::string kCurrentText = "current text";
  TranscriptBuilder builder(kSessionId, kSenderEmail);
  mojom::BabelOrcaMessagePtr message1 =
      CreateMessage(GetParam().opts1,
                    /*previous_transcript=*/CreateTranscriptPart({}),
                    /*current_transcript=*/CreateTranscriptPart({}));
  mojom::BabelOrcaMessagePtr message2 =
      CreateMessage(GetParam().opts2,
                    /*previous_transcript=*/CreateTranscriptPart({}),
                    /*current_transcript=*/CreateTranscriptPart({}));

  std::vector<TranscriptBuilder::Result> results1 =
      builder.GetTranscripts(std::move(message1));
  std::vector<TranscriptBuilder::Result> results2 =
      builder.GetTranscripts(std::move(message2));

  EXPECT_THAT(results1, testing::SizeIs(1));
  EXPECT_THAT(results2, testing::IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(
    TranscriptBuilderDiscardNextTestSuite,
    TranscriptBuilderDiscardNextTest,
    testing::ValuesIn<TranscriptBuilderDiscardNextTestCase>({
        {"LowerInitTimestamp",
         {.init_timestamp_ms = 5},
         {.init_timestamp_ms = 4}},
        {"SameOrder", {.order = 11}, {.order = 11}},
        {"LowerOrder", {.order = 11}, {.order = 10}},
    }),
    [](const testing::TestParamInfo<
        TranscriptBuilderDiscardNextTest::ParamType>& info) {
      return info.param.test_name;
    });

struct TranscriptBuilderUpdateTestCase {
  std::string test_name;
  int64_t index1;
  int64_t index2;
  std::string expcted_full_text;
};

using TranscriptBuilderUpdateTest =
    testing::TestWithParam<TranscriptBuilderUpdateTestCase>;

TEST_P(TranscriptBuilderUpdateTest, CurrentTranscript) {
  const std::string kTextPart1 = "current... t";
  const std::string kTextPart2 = "t text";
  TranscriptBuilder builder(kSessionId, kSenderEmail);
  mojom::BabelOrcaMessagePtr message1 = CreateMessage(
      {.order = 8},
      /*previous_transcript=*/nullptr,
      /*current_transcript=*/
      CreateTranscriptPart(
          {.index = GetParam().index1, .text = kTextPart1, .is_final = false}));
  mojom::BabelOrcaMessagePtr message2 = CreateMessage(
      {.order = 9},
      /*previous_transcript=*/nullptr,
      /*current_transcript=*/
      CreateTranscriptPart(
          {.index = GetParam().index2, .text = kTextPart2, .is_final = false}));

  std::vector<TranscriptBuilder::Result> results1 =
      builder.GetTranscripts(std::move(message1));
  std::vector<TranscriptBuilder::Result> results2 =
      builder.GetTranscripts(std::move(message2));

  ASSERT_THAT(results1, testing::SizeIs(1));
  VerifyBuilderResult(results1[0],
                      TranscriptBuilder::Result(
                          kTextPart1, /*is_final_param=*/false, kLanguageEn));

  ASSERT_THAT(results2, testing::SizeIs(1));
  VerifyBuilderResult(results2[0], TranscriptBuilder::Result(
                                       GetParam().expcted_full_text,
                                       /*is_final_param=*/false, kLanguageEn));
}

TEST_P(TranscriptBuilderUpdateTest, PreviousTranscript) {
  const std::string kTextPart1 = "current... t";
  const std::string kTextPart2 = "t text";
  const std::string kNewTextPart1 = "Last received";
  const std::string kNewTextPart2 = "Last received Updated";
  TranscriptBuilder builder(kSessionId, kSenderEmail);
  mojom::BabelOrcaMessagePtr message1 =
      CreateMessage({.order = 8},
                    /*previous_transcript=*/nullptr,
                    /*current_transcript=*/
                    CreateTranscriptPart({.id = 33,
                                          .index = GetParam().index1,
                                          .text = kTextPart1,
                                          .is_final = false}));
  TranscriptPartOptions prev_opts = {.id = 33,
                                     .index = GetParam().index2,
                                     .text = kTextPart2,
                                     .is_final = true};
  mojom::BabelOrcaMessagePtr message2 = CreateMessage(
      {.order = 9},
      /*previous_transcript=*/
      CreateTranscriptPart(prev_opts),
      /*current_transcript=*/
      CreateTranscriptPart(
          {.id = 34, .index = 0, .text = kNewTextPart1, .is_final = false}));
  mojom::BabelOrcaMessagePtr message3 = CreateMessage(
      {.order = 10},
      /*previous_transcript=*/
      CreateTranscriptPart(prev_opts),
      /*current_transcript=*/
      CreateTranscriptPart(
          {.id = 34, .index = 0, .text = kNewTextPart2, .is_final = false}));

  std::vector<TranscriptBuilder::Result> results1 =
      builder.GetTranscripts(std::move(message1));
  std::vector<TranscriptBuilder::Result> results2 =
      builder.GetTranscripts(std::move(message2));
  std::vector<TranscriptBuilder::Result> results3 =
      builder.GetTranscripts(std::move(message3));

  ASSERT_THAT(results1, testing::SizeIs(1));
  VerifyBuilderResult(results1[0],
                      TranscriptBuilder::Result(
                          kTextPart1, /*is_final_param=*/false, kLanguageEn));

  ASSERT_THAT(results2, testing::SizeIs(2));
  VerifyBuilderResult(results2[0], TranscriptBuilder::Result(
                                       GetParam().expcted_full_text,
                                       /*is_final_param=*/true, kLanguageEn));
  VerifyBuilderResult(results2[1], TranscriptBuilder::Result(
                                       kNewTextPart1,
                                       /*is_final_param=*/false, kLanguageEn));

  ASSERT_THAT(results3, testing::SizeIs(1));
  VerifyBuilderResult(
      results3[0], TranscriptBuilder::Result(
                       kNewTextPart2, /*is_final_param=*/false, kLanguageEn));
}

INSTANTIATE_TEST_SUITE_P(
    TranscriptBuilderUpdateTestSuite,
    TranscriptBuilderUpdateTest,
    testing::ValuesIn<TranscriptBuilderUpdateTestCase>(
        {{"Overlap", 0, 6, "current text"},
         {"OverlapStartMissed", 5, 11, "current text"},
         {"Append", 0, 12, "current... tt text"},
         {"AppendStartMissed", 5, 17, "current... tt text"}}),
    [](const testing::TestParamInfo<TranscriptBuilderUpdateTest::ParamType>&
           info) { return info.param.test_name; });

TEST(TranscriptBuilderTest, ChangeInitTimestampAfterFinalTranscript) {
  const std::string kTextPart1 = "first";
  const std::string kTextPart2 = "second";
  TranscriptBuilder builder(kSessionId, kSenderEmail);
  mojom::BabelOrcaMessagePtr message1 = CreateMessage(
      {.init_timestamp_ms = 50, .order = 5},
      /*previous_transcript=*/nullptr,
      /*current_transcript=*/
      CreateTranscriptPart(
          {.id = 1, .index = 0, .text = kTextPart1, .is_final = true}));
  mojom::BabelOrcaMessagePtr message2 = CreateMessage(
      {.init_timestamp_ms = 100, .order = 4},
      /*previous_transcript=*/nullptr,
      /*current_transcript=*/
      CreateTranscriptPart(
          {.id = 2, .index = 6, .text = kTextPart2, .is_final = false}));

  std::vector<TranscriptBuilder::Result> results1 =
      builder.GetTranscripts(std::move(message1));
  std::vector<TranscriptBuilder::Result> results2 =
      builder.GetTranscripts(std::move(message2));

  ASSERT_THAT(results1, testing::SizeIs(1));
  VerifyBuilderResult(results1[0],
                      TranscriptBuilder::Result(
                          kTextPart1, /*is_final_param=*/true, kLanguageEn));

  ASSERT_THAT(results2, testing::SizeIs(1));
  VerifyBuilderResult(results2[0],
                      TranscriptBuilder::Result(
                          kTextPart2, /*is_final_param=*/false, kLanguageEn));
}

TEST(TranscriptBuilderTest, ChangeInitTimestamp) {
  const std::string kTextPart1 = "first";
  const std::string kTextPart2 = "second";
  TranscriptBuilder builder(kSessionId, kSenderEmail);
  mojom::BabelOrcaMessagePtr message1 = CreateMessage(
      {.init_timestamp_ms = 50, .order = 5},
      /*previous_transcript=*/nullptr,
      /*current_transcript=*/
      CreateTranscriptPart(
          {.id = 1, .index = 0, .text = kTextPart1, .is_final = false}));
  mojom::BabelOrcaMessagePtr message2 = CreateMessage(
      {.init_timestamp_ms = 100, .order = 4},
      /*previous_transcript=*/nullptr,
      /*current_transcript=*/
      CreateTranscriptPart(
          {.id = 2, .index = 6, .text = kTextPart2, .is_final = false}));

  std::vector<TranscriptBuilder::Result> results1 =
      builder.GetTranscripts(std::move(message1));
  std::vector<TranscriptBuilder::Result> results2 =
      builder.GetTranscripts(std::move(message2));

  ASSERT_THAT(results1, testing::SizeIs(1));
  VerifyBuilderResult(results1[0],
                      TranscriptBuilder::Result(
                          kTextPart1, /*is_final_param=*/false, kLanguageEn));

  ASSERT_THAT(results2, testing::SizeIs(2));
  VerifyBuilderResult(results2[0],
                      TranscriptBuilder::Result(
                          kTextPart1, /*is_final_param=*/true, kLanguageEn));
  VerifyBuilderResult(results2[1],
                      TranscriptBuilder::Result(
                          kTextPart2, /*is_final_param=*/false, kLanguageEn));
}

TEST(TranscriptBuilderTest, MissingTextCurrent) {
  const std::string kTextPart1 = "first";
  const std::string kTextPart2 = "second";
  TranscriptBuilder builder(kSessionId, kSenderEmail);
  mojom::BabelOrcaMessagePtr message1 = CreateMessage(
      {.order = 5},
      /*previous_transcript=*/nullptr,
      /*current_transcript=*/
      CreateTranscriptPart(
          {.id = 1, .index = 0, .text = kTextPart1, .is_final = false}));
  mojom::BabelOrcaMessagePtr message2 = CreateMessage(
      {.order = 6},
      /*previous_transcript=*/nullptr,
      /*current_transcript=*/
      CreateTranscriptPart(
          {.id = 1, .index = 6, .text = kTextPart2, .is_final = false}));

  std::vector<TranscriptBuilder::Result> results1 =
      builder.GetTranscripts(std::move(message1));
  std::vector<TranscriptBuilder::Result> results2 =
      builder.GetTranscripts(std::move(message2));

  ASSERT_THAT(results1, testing::SizeIs(1));
  VerifyBuilderResult(results1[0],
                      TranscriptBuilder::Result(
                          kTextPart1, /*is_final_param=*/false, kLanguageEn));

  ASSERT_THAT(results2, testing::SizeIs(2));
  VerifyBuilderResult(results2[0],
                      TranscriptBuilder::Result(
                          kTextPart1, /*is_final_param=*/true, kLanguageEn));
  VerifyBuilderResult(results2[1],
                      TranscriptBuilder::Result(
                          kTextPart2, /*is_final_param=*/false, kLanguageEn));
}

TEST(TranscriptBuilderTest, MissingTextPrevious) {
  const std::string kTextPart1 = "first";
  const std::string kTextPart2 = "second";
  const std::string kTextPart3 = "third";
  TranscriptBuilder builder(kSessionId, kSenderEmail);
  mojom::BabelOrcaMessagePtr message1 = CreateMessage(
      {.order = 5},
      /*previous_transcript=*/nullptr,
      /*current_transcript=*/
      CreateTranscriptPart(
          {.id = 1, .index = 0, .text = kTextPart1, .is_final = false}));
  mojom::BabelOrcaMessagePtr message2 = CreateMessage(
      {.order = 6},
      /*previous_transcript=*/
      CreateTranscriptPart(
          {.id = 1, .index = 6, .text = kTextPart2, .is_final = true}),
      /*current_transcript=*/
      CreateTranscriptPart(
          {.id = 2, .index = 0, .text = kTextPart3, .is_final = false}));

  std::vector<TranscriptBuilder::Result> results1 =
      builder.GetTranscripts(std::move(message1));
  std::vector<TranscriptBuilder::Result> results2 =
      builder.GetTranscripts(std::move(message2));

  ASSERT_THAT(results1, testing::SizeIs(1));
  VerifyBuilderResult(results1[0],
                      TranscriptBuilder::Result(
                          kTextPart1, /*is_final_param=*/false, kLanguageEn));

  ASSERT_THAT(results2, testing::SizeIs(3));
  VerifyBuilderResult(results2[0],
                      TranscriptBuilder::Result(
                          kTextPart1, /*is_final_param=*/true, kLanguageEn));
  VerifyBuilderResult(results2[1],
                      TranscriptBuilder::Result(
                          kTextPart2, /*is_final_param=*/true, kLanguageEn));
  VerifyBuilderResult(results2[2],
                      TranscriptBuilder::Result(
                          kTextPart3, /*is_final_param=*/false, kLanguageEn));
}

}  // namespace
}  // namespace ash::babelorca
