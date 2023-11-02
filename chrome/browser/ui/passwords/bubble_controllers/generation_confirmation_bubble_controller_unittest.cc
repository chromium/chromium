// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/generation_confirmation_bubble_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace {

constexpr char kUIDismissalReasonGeneralMetric[] =
    "PasswordManager.UIDismissalReason";

}  // namespace

class GenerationConfirmationBubbleControllerTest : public ::testing::Test {
 public:
  GenerationConfirmationBubbleControllerTest() {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
    ON_CALL(*mock_delegate_, GetPasswordFormMetricsRecorder())
        .WillByDefault(Return(nullptr));
  }
  ~GenerationConfirmationBubbleControllerTest() override = default;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  GenerationConfirmationBubbleController* controller() {
    return controller_.get();
  }

  void Init();
  void DestroyController();

 private:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<GenerationConfirmationBubbleController> controller_;
};

void GenerationConfirmationBubbleControllerTest::Init() {
  EXPECT_CALL(*delegate(), OnBubbleShown());
  controller_ = std::make_unique<GenerationConfirmationBubbleController>(
      mock_delegate_->AsWeakPtr(),
      PasswordBubbleControllerBase::DisplayReason::kAutomatic);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
}

void GenerationConfirmationBubbleControllerTest::DestroyController() {
  controller_.reset();
}

TEST_F(GenerationConfirmationBubbleControllerTest,
       NavigateToDashboardWithBubbleClosing) {
  Init();

  controller()->OnNavigateToPasswordManagerAccountDashboardLinkClicked(
      password_manager::ManagePasswordsReferrer::kManagePasswordsBubble);

  base::HistogramTester histogram_tester;

  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();

  DestroyController();

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonGeneralMetric,
      password_manager::metrics_util::CLICKED_PASSWORDS_DASHBOARD, 1);
}

TEST_F(GenerationConfirmationBubbleControllerTest,
       NavigateToDashboardWithoutBubbleClosing) {
  Init();

  controller()->OnNavigateToPasswordManagerAccountDashboardLinkClicked(
      password_manager::ManagePasswordsReferrer::kManagePasswordsBubble);

  base::HistogramTester histogram_tester;

  EXPECT_CALL(*delegate(), OnBubbleHidden());

  DestroyController();

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonGeneralMetric,
      password_manager::metrics_util::CLICKED_PASSWORDS_DASHBOARD, 1);
}
