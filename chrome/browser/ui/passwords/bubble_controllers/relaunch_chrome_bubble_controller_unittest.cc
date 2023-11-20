// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/relaunch_chrome_bubble_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kAcceptHistogram[] =
    "PasswordBubble.RelaunchChromeBubble.RestartButtonInBubbleClicked";
}

class RelaunchChromeBubbleControllerTest : public ::testing::Test {
 public:
  RelaunchChromeBubbleControllerTest() {
    test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    test_pref_service_->registry()->RegisterIntegerPref(
        password_manager::prefs::kRelaunchChromeBubbleDismissedCounter, 0);
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
  }
  ~RelaunchChromeBubbleControllerTest() override = default;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  RelaunchChromeBubbleController* controller() { return controller_.get(); }

  TestingPrefServiceSimple* test_pref_service() {
    return test_pref_service_.get();
  }

  void CreateController() {
    EXPECT_CALL(*delegate(), OnBubbleShown());
    controller_ = std::make_unique<RelaunchChromeBubbleController>(
        mock_delegate_->AsWeakPtr(), test_pref_service_.get());
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  }

  void ResetController() { controller_.reset(); }

  void CancelAndClose() {
    controller()->OnCanceled();
    controller()->OnBubbleClosing();
    ResetController();
  }

  void CreateCancelAndClose() {
    CreateController();
    CancelAndClose();
  }

 private:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
  std::unique_ptr<RelaunchChromeBubbleController> controller_;
};

TEST_F(RelaunchChromeBubbleControllerTest, Destroy) {
  base::HistogramTester histograms;
  CreateController();

  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();
  ResetController();
  histograms.ExpectUniqueSample(kAcceptHistogram, false, 1);
}

TEST_F(RelaunchChromeBubbleControllerTest, Content) {
  CreateController();
  EXPECT_NE(std::u16string(), controller()->GetTitle());
  EXPECT_NE(std::u16string(), controller()->GetBody());
  EXPECT_NE(std::u16string(), controller()->GetContinueButtonText());
  EXPECT_NE(std::u16string(), controller()->GetNoThanksButtonText());
}

TEST_F(RelaunchChromeBubbleControllerTest, Cancel) {
  base::HistogramTester histograms;
  CreateCancelAndClose();

  EXPECT_EQ(test_pref_service()->GetInteger(
                password_manager::prefs::kRelaunchChromeBubbleDismissedCounter),
            1);
  histograms.ExpectUniqueSample(kAcceptHistogram, false, 1);
}

TEST_F(RelaunchChromeBubbleControllerTest, Restart) {
  base::HistogramTester histograms;
  CreateController();

  EXPECT_CALL(*delegate(), RelaunchChrome);
  controller()->OnAccepted();
  controller()->OnBubbleClosing();
  ResetController();

  histograms.ExpectUniqueSample(kAcceptHistogram, true, 1);
}

TEST_F(RelaunchChromeBubbleControllerTest, DontAskAgain) {
  base::HistogramTester histograms;

  CreateController();
  std::u16string initial_cancel_button_text =
      controller()->GetNoThanksButtonText();
  EXPECT_NE(std::u16string(), initial_cancel_button_text);
  CancelAndClose();

  // Simulate that the bubble was shown and cancelled 2 additional times,
  // meaning than the next showup should be it's last.
  test_pref_service()->SetInteger(
      password_manager::prefs::kRelaunchChromeBubbleDismissedCounter, 3);

  CreateController();
  std::u16string never_button_text = controller()->GetNoThanksButtonText();
  EXPECT_NE(std::u16string(), never_button_text);
  CancelAndClose();

  EXPECT_NE(never_button_text, initial_cancel_button_text);
  histograms.ExpectUniqueSample(kAcceptHistogram, false, 2);
}
