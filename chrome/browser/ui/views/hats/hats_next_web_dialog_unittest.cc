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
