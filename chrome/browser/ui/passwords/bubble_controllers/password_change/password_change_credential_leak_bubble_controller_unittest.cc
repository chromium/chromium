// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_credential_leak_bubble_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate_mock.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
namespace metrics_util = password_manager::metrics_util;

class PasswordChangeCredentialLeakBubbleControllerTest
    : public ::testing::Test {
 public:
  void CreateController() {
    EXPECT_CALL(mock_delegate_, OnBubbleShown());
    ON_CALL(mock_delegate_, GetPasswordChangeDelegate)
        .WillByDefault(Return(&password_change_delegate_));
    ON_CALL(mock_delegate_, GetPasswordsLeakDialogDelegate)
        .WillByDefault(Return(&passwords_leak_dialog_delegate_));
    controller_ =
        std::make_unique<PasswordChangeCredentialLeakBubbleController>(
            mock_delegate_.AsWeakPtr());
  }

 protected:
  PasswordsModelDelegateMock mock_delegate_;
  std::unique_ptr<PasswordChangeCredentialLeakBubbleController> controller_;
  PasswordChangeDelegateMock password_change_delegate_;
  PasswordsLeakDialogDelegateMock passwords_leak_dialog_delegate_;
};

TEST_F(PasswordChangeCredentialLeakBubbleControllerTest,
       MetricsReportedForCancel) {
  base::HistogramTester histogram_tester;
  CreateController();

  controller_->Cancel();
  controller_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.LeakDetectionBubble",
      metrics_util::CLICKED_CANCEL, 1);
}

TEST_F(PasswordChangeCredentialLeakBubbleControllerTest,
       MetricsReportedForAccept) {
  base::HistogramTester histogram_tester;
  CreateController();

  controller_->ChangePassword();
  controller_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.LeakDetectionBubble",
      metrics_util::CLICKED_ACCEPT, 1);
}

TEST_F(PasswordChangeCredentialLeakBubbleControllerTest,
       MetricsReportedForAboutPasswordChange) {
  base::HistogramTester histogram_tester;
  CreateController();

  controller_->NavigateToPasswordChangeSettings();
  controller_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.LeakDetectionBubble",
      metrics_util::CLICKED_ABOUT_PASSWORD_CHANGE, 1);
}
