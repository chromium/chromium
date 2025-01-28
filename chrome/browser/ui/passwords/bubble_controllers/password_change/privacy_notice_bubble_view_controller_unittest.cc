// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/privacy_notice_bubble_view_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_info_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
namespace metrics_util = password_manager::metrics_util;

class PrivacyNoticeBubbleViewControllerTest : public ::testing::Test {
 public:
  PrivacyNoticeBubbleViewControllerTest() {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
  }
  void CreateController() {
    EXPECT_CALL(*mock_delegate_, OnBubbleShown());
    password_change_delegate_ = std::make_unique<PasswordChangeDelegateMock>();
    ON_CALL(*mock_delegate_, GetPasswordChangeDelegate)
        .WillByDefault(Return(password_change_delegate_.get()));
    controller_ = std::make_unique<PrivacyNoticeBubbleViewController>(
        mock_delegate_->AsWeakPtr());
  }

 protected:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<PrivacyNoticeBubbleViewController> controller_;
  std::unique_ptr<PasswordChangeDelegateMock> password_change_delegate_;
};

TEST_F(PrivacyNoticeBubbleViewControllerTest, MetricsReportedForCancel) {
  base::HistogramTester histogram_tester;
  CreateController();

  controller_->Cancel();
  controller_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.PrivacyNoticeBubble",
      metrics_util::CLICKED_CANCEL, 1);
}

TEST_F(PrivacyNoticeBubbleViewControllerTest, MetricsReportedForAccept) {
  base::HistogramTester histogram_tester;
  CreateController();

  controller_->AcceptNotice();
  controller_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.PrivacyNoticeBubble",
      metrics_util::CLICKED_ACCEPT, 1);
}
