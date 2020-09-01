// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/kp_entity_result_parser.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_answers {
namespace {
using base::Value;
}

class KpEntityResultParserTest : public testing::Test {
 public:
  KpEntityResultParserTest()
      : parser_(std::make_unique<KpEntityResultParser>()) {}

  KpEntityResultParserTest(const KpEntityResultParserTest&) = delete;
  KpEntityResultParserTest& operator=(const KpEntityResultParserTest&) = delete;

 protected:
  std::unique_ptr<KpEntityResultParser> parser_;
};

TEST_F(KpEntityResultParserTest, SuccessWithRating) {
  Value result(Value::Type::DICTIONARY);
  result.SetDoublePath(
      "knowledgePanelEntityResult.entity.ratingsAndReviews.google."
      "aggregateRating.averageScore",
      4.5);
  result.SetStringPath(
      "knowledgePanelEntityResult.entity.ratingsAndReviews.google."
      "aggregateRating.aggregatedCount",
      "100");

  QuickAnswer quick_answer;

  EXPECT_TRUE(parser_->Parse(&result, &quick_answer));
  EXPECT_EQ(ResultType::kKnowledgePanelEntityResult, quick_answer.result_type);
  EXPECT_EQ("4.5 ★ (100 reviews)", quick_answer.primary_answer);
  EXPECT_TRUE(quick_answer.secondary_answer.empty());

  EXPECT_EQ(0u, quick_answer.title.size());
  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  EXPECT_EQ(
      base::UTF8ToUTF16("4.5 ★ (100 reviews)"),
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get())
          ->text);
}

TEST_F(KpEntityResultParserTest, SuccessWithRatingScoreRound) {
  Value result(Value::Type::DICTIONARY);
  result.SetDoublePath(
      "knowledgePanelEntityResult.entity.ratingsAndReviews.google."
      "aggregateRating.averageScore",
      4.52);
  result.SetStringPath(
      "knowledgePanelEntityResult.entity.ratingsAndReviews.google."
      "aggregateRating.aggregatedCount",
      "100");

  QuickAnswer quick_answer;

  EXPECT_TRUE(parser_->Parse(&result, &quick_answer));
  EXPECT_EQ(ResultType::kKnowledgePanelEntityResult, quick_answer.result_type);
  EXPECT_EQ("4.5 ★ (100 reviews)", quick_answer.primary_answer);
  EXPECT_TRUE(quick_answer.secondary_answer.empty());

  EXPECT_EQ(0u, quick_answer.title.size());
  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ(base::UTF8ToUTF16("4.5 ★ (100 reviews)"), answer->text);
  EXPECT_EQ(gfx::kGoogleGrey700, answer->color);

  result.SetDoublePath(
      "knowledgePanelEntityResult.entity.ratingsAndReviews.google."
      "aggregateRating.averageScore",
      4.56);

  QuickAnswer quick_answer2;
  EXPECT_TRUE(parser_->Parse(&result, &quick_answer2));
  EXPECT_EQ("4.6 ★ (100 reviews)", quick_answer2.primary_answer);

  EXPECT_EQ(0u, quick_answer2.title.size());
  EXPECT_EQ(1u, quick_answer2.first_answer_row.size());
  answer =
      static_cast<QuickAnswerText*>(quick_answer2.first_answer_row[0].get());
  EXPECT_EQ(base::UTF8ToUTF16("4.6 ★ (100 reviews)"), answer->text);
  EXPECT_EQ(gfx::kGoogleGrey700, answer->color);
}

TEST_F(KpEntityResultParserTest, SuccessWithKnownForReason) {
  Value result(Value::Type::DICTIONARY);
  result.SetStringPath(
      "knowledgePanelEntityResult.entity.localizedKnownForReason",
      "44th U.S. President");

  QuickAnswer quick_answer;

  EXPECT_TRUE(parser_->Parse(&result, &quick_answer));
  EXPECT_EQ(ResultType::kKnowledgePanelEntityResult, quick_answer.result_type);
  EXPECT_EQ("44th U.S. President", quick_answer.primary_answer);
  EXPECT_TRUE(quick_answer.secondary_answer.empty());

  EXPECT_EQ(0u, quick_answer.title.size());
  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ(base::UTF8ToUTF16("44th U.S. President"), answer->text);
  EXPECT_EQ(gfx::kGoogleGrey700, answer->color);
}

TEST_F(KpEntityResultParserTest, EmptyValue) {
  Value result(Value::Type::DICTIONARY);
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

TEST_F(KpEntityResultParserTest, IncorrectType) {
  Value result(Value::Type::DICTIONARY);
  result.SetIntPath("ratingsAndReviews.google.aggregateRating.aggregatedCount",
                    100);
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

TEST_F(KpEntityResultParserTest, IncorrectPath) {
  Value result(Value::Type::DICTIONARY);
  result.SetStringPath(
      "ratingsAndReviews.google.aggregateRating.aggregatedCounts", "100");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

}  // namespace quick_answers
}  // namespace chromeos
