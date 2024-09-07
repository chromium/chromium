// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hats/hats_next_web_dialog.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class HatsNextWebDialogTest : public testing::Test {
 public:
  HatsNextWebDialogTest() = default;
};

TEST_F(HatsNextWebDialogTest, ParseSurveyQuestionAnswer) {
  int question;
  std::vector<int> answers;

  // Incomplete answers
  EXPECT_FALSE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer-", &question, &answers));
  EXPECT_FALSE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer-1", &question, &answers));
  EXPECT_FALSE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer-1-", &question, &answers));

  // Invalid integers.
  EXPECT_FALSE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer-a-1,2,3", &question, &answers));
  EXPECT_FALSE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer-1-a", &question, &answers));

  // Out of range
  EXPECT_FALSE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer--1-1,2,3", &question, &answers));
  EXPECT_FALSE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer-1--1", &question, &answers));
  EXPECT_FALSE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer-0-1,2,3", &question, &answers));
  EXPECT_FALSE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer-11-1", &question, &answers));
  EXPECT_FALSE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer-1-101", &question, &answers));

  // Overflow int.
  EXPECT_FALSE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer-2147483648-a", &question, &answers));
  EXPECT_FALSE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer-1-2147483648", &question, &answers));

  EXPECT_TRUE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer-1-10", &question, &answers));
  EXPECT_EQ(question, 1);
  EXPECT_EQ(answers.size(), 1UL);
  EXPECT_EQ(answers[0], 10);

  answers.clear();
  EXPECT_TRUE(HatsNextWebDialog::ParseSurveyQuestionAnswer(
      "answer-2-1,2", &question, &answers));
  EXPECT_EQ(question, 2);
  EXPECT_EQ(answers.size(), 2UL);
  EXPECT_EQ(answers[0], 1);
  EXPECT_EQ(answers[1], 2);
}

TEST_F(HatsNextWebDialogTest, EncodeUkmQuestionAnswers) {
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({0}),
            static_cast<uint64_t>(0));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({}),
            static_cast<uint64_t>(0));

  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({1}),
            static_cast<uint64_t>(0b1));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({2}),
            static_cast<uint64_t>(0b10));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({1, 2}),
            static_cast<uint64_t>(0b11));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({3}),
            static_cast<uint64_t>(0b100));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({1, 3}),
            static_cast<uint64_t>(0b101));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({2, 3}),
            static_cast<uint64_t>(0b110));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({1, 2, 3}),
            static_cast<uint64_t>(0b111));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({4}),
            static_cast<uint64_t>(0b1000));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({1, 4}),
            static_cast<uint64_t>(0b1001));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({2, 4}),
            static_cast<uint64_t>(0b1010));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({1, 2, 4}),
            static_cast<uint64_t>(0b1011));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({3, 4}),
            static_cast<uint64_t>(0b1100));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({1, 3, 4}),
            static_cast<uint64_t>(0b1101));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({2, 3, 4}),
            static_cast<uint64_t>(0b1110));
  EXPECT_EQ(HatsNextWebDialog::EncodeUkmQuestionAnswers({1, 2, 3, 4}),
            static_cast<uint64_t>(0b1111));
}
