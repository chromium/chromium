// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/no_password_change_form_bubble_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
namespace metrics_util = password_manager::metrics_util;

class NoPasswordChangeFormBubbleControllerTest : public ::testing::Test {
 public:
  void CreateController() {
    EXPECT_CALL(mock_delegate_, OnBubbleShown());
    ON_CALL(mock_delegate_, GetPasswordChangeDelegate)
        .WillByDefault(Return(&password_change_delegate_));
    controller_ = std::make_unique<NoPasswordChangeFormBubbleController>(
        mock_delegate_.AsWeakPtr());
  }

 protected:
  PasswordsModelDelegateMock mock_delegate_;
  std::unique_ptr<NoPasswordChangeFormBubbleController> controller_;
  PasswordChangeDelegateMock password_change_delegate_;
};

TEST_F(NoPasswordChangeFormBubbleControllerTest, MetricsReportedForCancel) {
  base::HistogramTester histogram_tester;
  CreateController();

  controller_->Cancel();
  controller_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.NoPasswordFormBubble",
      metrics_util::CLICKED_CANCEL, 1);
}

TEST_F(NoPasswordChangeFormBubbleControllerTest, MetricsReportedForRetry) {
  base::HistogramTester histogram_tester;
  CreateController();

  controller_->Restart();
  controller_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.NoPasswordFormBubble",
      metrics_util::CLICKED_ACCEPT, 1);
}
