// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/kp_entity_result_parser.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"

namespace quick_answers {
namespace {
using base::Value;
}  // namespace

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
  Value::Dict result;
  result.SetByDottedPath(
      "knowledgePanelEntityResult.entity.ratingsAndReviews.google."
      "aggregateRating.averageScore",
      4.5);
  result.SetByDottedPath(
      "knowledgePanelEntityResult.entity.ratingsAndReviews.google."
      "aggregateRating.aggregatedCount",
      "100");

  QuickAnswer quick_answer;

  EXPECT_TRUE(parser_->Parse(result, &quick_answer));

  EXPECT_EQ(0u, quick_answer.title.size());
  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  EXPECT_EQ("4.5 ★ (100 reviews)",
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
}

TEST_F(KpEntityResultParserTest, SuccessWithRatingScoreRound) {
  Value::Dict result;
  result.SetByDottedPath(
      "knowledgePanelEntityResult.entity.ratingsAndReviews.google."
      "aggregateRating.averageScore",
      4.52);
  result.SetByDottedPath(
      "knowledgePanelEntityResult.entity.ratingsAndReviews.google."
      "aggregateRating.aggregatedCount",
      "100");

  QuickAnswer quick_answer;

  EXPECT_TRUE(parser_->Parse(result, &quick_answer));

  EXPECT_EQ(0u, quick_answer.title.size());
  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ("4.5 ★ (100 reviews)",
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
  EXPECT_EQ(ui::kColorLabelForegroundSecondary, answer->color_id);

  result.SetByDottedPath(
      "knowledgePanelEntityResult.entity.ratingsAndReviews.google."
      "aggregateRating.averageScore",
      4.56);

  QuickAnswer quick_answer2;
  EXPECT_TRUE(parser_->Parse(result, &quick_answer2));

  EXPECT_EQ(0u, quick_answer2.title.size());
  EXPECT_EQ(1u, quick_answer2.first_answer_row.size());
  answer =
      static_cast<QuickAnswerText*>(quick_answer2.first_answer_row[0].get());
  EXPECT_EQ("4.5 ★ (100 reviews)",
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
  EXPECT_EQ(ui::kColorLabelForegroundSecondary, answer->color_id);
}

TEST_F(KpEntityResultParserTest, SuccessWithKnownForReason) {
  Value::Dict result;
  result.SetByDottedPath(
      "knowledgePanelEntityResult.entity.localizedKnownForReason",
      "44th U.S. President");

  QuickAnswer quick_answer;

  EXPECT_TRUE(parser_->Parse(result, &quick_answer));

  EXPECT_EQ(0u, quick_answer.title.size());
  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ("44th U.S. President",
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
  EXPECT_EQ(ui::kColorLabelForegroundSecondary, answer->color_id);
}

TEST_F(KpEntityResultParserTest, EmptyValue) {
  Value::Dict result;
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(result, &quick_answer));
}

TEST_F(KpEntityResultParserTest, IncorrectType) {
  Value::Dict result;
  result.SetByDottedPath(
      "ratingsAndReviews.google.aggregateRating.aggregatedCount", 100);
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(result, &quick_answer));
}

TEST_F(KpEntityResultParserTest, IncorrectPath) {
  Value::Dict result;
  result.SetByDottedPath(
      "ratingsAndReviews.google.aggregateRating.aggregatedCounts", "100");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(result, &quick_answer));
}

}  // namespace quick_answers
