// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_browsertest_base.h"

#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "content/public/test/browser_test.h"

namespace {

using QuickAnswersControllerTest = quick_answers::QuickAnswersBrowserTestBase;

}  // namespace

IN_PROC_BROWSER_TEST_P(QuickAnswersControllerTest, FeatureIneligible) {
  QuickAnswersState::Get()->SetEligibilityForTesting(false);

  ShowMenuParams params;
  params.selected_text = "test";

  ShowMenuAndWait(params);

  // Quick Answers UI should stay hidden since the feature is not eligible.
  ASSERT_EQ(QuickAnswersVisibility::kClosed,
            controller()->GetQuickAnswersVisibility());
}

IN_PROC_BROWSER_TEST_P(QuickAnswersControllerTest, PasswordField) {
  QuickAnswersState::Get()->SetEligibilityForTesting(true);

  ShowMenuParams params;
  params.selected_text = "test";
  params.is_password_field = true;

  ShowMenuAndWait(params);

  // Quick Answers UI should stay hidden since the input field is password
  // field.
  ASSERT_EQ(QuickAnswersVisibility::kClosed,
            controller()->GetQuickAnswersVisibility());
}

IN_PROC_BROWSER_TEST_P(QuickAnswersControllerTest, NoSelectedText) {
  QuickAnswersState::Get()->SetEligibilityForTesting(true);

  ShowMenuAndWait(ShowMenuParams());

  // Quick Answers UI should stay hidden since no text is selected.
  ASSERT_EQ(QuickAnswersVisibility::kClosed,
            controller()->GetQuickAnswersVisibility());
}

IN_PROC_BROWSER_TEST_P(QuickAnswersControllerTest, QuickAnswersPending) {
  if (IsMagicBoostEnabled()) {
    GTEST_SKIP() << "This test only applies when Magic Boost is disabled.";
  }

  QuickAnswersState::Get()->SetEligibilityForTesting(true);

  ShowMenuParams params;
  params.selected_text = "test";
  ShowMenuAndWait(params);

  // Quick Answers UI should be pending.
  ASSERT_EQ(QuickAnswersVisibility::kPending,
            controller()->GetQuickAnswersVisibility());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    QuickAnswersControllerTest,
    ::testing::Bool());
