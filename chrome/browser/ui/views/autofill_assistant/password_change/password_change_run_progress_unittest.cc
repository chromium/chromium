// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_progress.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

using ::testing::StrictMock;

class PasswordChangeRunProgressTest : public views::ViewsTestBase {
 public:
  PasswordChangeRunProgressTest() = default;
  ~PasswordChangeRunProgressTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    password_change_run_progress_ =
        widget_->SetContentsView(std::make_unique<PasswordChangeRunProgress>());
  }
  PasswordChangeRunProgress* get_password_change_run_progress() {
    return password_change_run_progress_;
  }

  void TearDown() override {
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

 private:
  raw_ptr<PasswordChangeRunProgress> password_change_run_progress_;
  // Widget to anchor the view and retrieve a color provider from.
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(PasswordChangeRunProgressTest, SetProgress) {
  PasswordChangeRunProgress* password_change_run_progress =
      get_password_change_run_progress();
  EXPECT_EQ(
      password_change_run_progress->GetCurrentProgressBarStep(),
      autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_START);
  password_change_run_progress->SetProgressBarStep(
      autofill_assistant::password_change::ProgressStep::
          PROGRESS_STEP_CHANGE_PASSWORD);

  EXPECT_EQ(password_change_run_progress->GetCurrentProgressBarStep(),
            autofill_assistant::password_change::ProgressStep::
                PROGRESS_STEP_CHANGE_PASSWORD);
}

TEST_F(PasswordChangeRunProgressTest, CannotSetPriorProgressStep) {
  PasswordChangeRunProgress* password_change_run_progress =
      get_password_change_run_progress();

  password_change_run_progress->SetProgressBarStep(
      autofill_assistant::password_change::ProgressStep::
          PROGRESS_STEP_CHANGE_PASSWORD);
  password_change_run_progress->SetProgressBarStep(
      autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_START);

  EXPECT_EQ(password_change_run_progress->GetCurrentProgressBarStep(),
            autofill_assistant::password_change::ProgressStep::
                PROGRESS_STEP_CHANGE_PASSWORD);
}
