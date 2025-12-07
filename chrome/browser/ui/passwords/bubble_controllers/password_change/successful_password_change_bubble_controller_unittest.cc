// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/successful_password_change_bubble_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
namespace metrics_util = password_manager::metrics_util;

class SuccessfulPasswordChangeBubbleControllerTest : public ::testing::Test {
 public:
  void CreateController() {
    EXPECT_CALL(mock_delegate_, OnBubbleShown());
    controller_ = std::make_unique<SuccessfulPasswordChangeBubbleController>(
        mock_delegate_.AsWeakPtr());
  }

 protected:
  PasswordsModelDelegateMock mock_delegate_;
  std::unique_ptr<SuccessfulPasswordChangeBubbleController> controller_;
};

TEST_F(SuccessfulPasswordChangeBubbleControllerTest,
       MetricsReportedForFinishPasswordChange) {
  base::HistogramTester histogram_tester;
  CreateController();

  controller_->FinishPasswordChange();
  controller_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.SuccessBubble",
      metrics_util::CLICKED_ACCEPT, 1);
}

TEST_F(SuccessfulPasswordChangeBubbleControllerTest,
       MetricsReportedForOpenPasswordManager) {
  base::HistogramTester histogram_tester;
  CreateController();

  controller_->OpenPasswordManager();
  controller_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.SuccessBubble",
      metrics_util::CLICKED_MANAGE_PASSWORD, 1);
}
