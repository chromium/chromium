// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_change_ui_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

using ::base::Bucket;
using ::testing::ElementsAre;

class PasswordChangeUIControllerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    tabs::TabInterface* tab_interface = browser()->GetActiveTabInterface();
    ASSERT_TRUE(tab_interface);
    ui_controller_ =
        std::make_unique<PasswordChangeUIController>(&delegate_, tab_interface);
  }

  void TearDownOnMainThread() override {
    // Needed to avoid dangling pointer to tab interface.
    ui_controller_ = nullptr;
  }

  void UpdateState(PasswordChangeDelegate::State state) {
    ui_controller_->UpdateState(state);
  }

  views::DialogDelegate* GetDialogDelegate() {
    return ui_controller_->dialog_widget()
        ->widget_delegate()
        ->AsDialogDelegate();
  }

  PasswordChangeUIController* ui_controller() { return ui_controller_.get(); }

  views::MdTextButton* GetToastActionButton() {
    return ui_controller_->toast_view()->action_button();
  }

  views::ImageButton* GetToastCloseButton() {
    return ui_controller_->toast_view()->close_button();
  }

 protected:
  base::HistogramTester histogram_tester_;
  PasswordChangeDelegateMock delegate_;
  std::unique_ptr<PasswordChangeUIController> ui_controller_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       OfferingPasswordChangeDialogAccepted) {
  UpdateState(PasswordChangeDelegate::State::kOfferingPasswordChange);

  EXPECT_CALL(delegate_, StartPasswordChangeFlow);
  GetDialogDelegate()->AcceptDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.LeakDetectionDialog",
      PasswordChangeDialogAction::kAcceptButtonClicked,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.LeakDetectionDialog.WithoutPrivacyNotice",
      PasswordChangeDialogAction::kAcceptButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       OfferingPasswordChangeDialogCancelled) {
  UpdateState(PasswordChangeDelegate::State::kOfferingPasswordChange);

  EXPECT_CALL(delegate_, Stop);
  GetDialogDelegate()->CancelDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.LeakDetectionDialog",
      PasswordChangeDialogAction::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.LeakDetectionDialog.WithoutPrivacyNotice",
      PasswordChangeDialogAction::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       PrivacyNoticeDialogAccepted) {
  UpdateState(PasswordChangeDelegate::State::kWaitingForAgreement);

  EXPECT_CALL(delegate_, OnPrivacyNoticeAccepted);
  GetDialogDelegate()->AcceptDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.LeakDetectionDialog",
      PasswordChangeDialogAction::kAcceptButtonClicked,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.LeakDetectionDialog.WithPrivacyNotice",
      PasswordChangeDialogAction::kAcceptButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       PrivacyNoticeDialogCancelled) {
  UpdateState(PasswordChangeDelegate::State::kWaitingForAgreement);

  EXPECT_CALL(delegate_, Stop);
  GetDialogDelegate()->CancelDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.LeakDetectionDialog",
      PasswordChangeDialogAction::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.LeakDetectionDialog.WithPrivacyNotice",
      PasswordChangeDialogAction::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       PasswordFormNotFoundDialogAccepted) {
  UpdateState(PasswordChangeDelegate::State::kChangePasswordFormNotFound);

  EXPECT_CALL(delegate_, OpenPasswordChangeTab);
  EXPECT_CALL(delegate_, Stop);
  GetDialogDelegate()->AcceptDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.NoPasswordForm",
      PasswordChangeDialogAction::kAcceptButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       PasswordFormNotFoundDialogCancelled) {
  UpdateState(PasswordChangeDelegate::State::kChangePasswordFormNotFound);

  EXPECT_CALL(delegate_, Stop);
  GetDialogDelegate()->CancelDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.NoPasswordForm",
      PasswordChangeDialogAction::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       ErrorDialogAccepted) {
  UpdateState(PasswordChangeDelegate::State::kPasswordChangeFailed);

  EXPECT_CALL(delegate_, OpenPasswordChangeTab);
  EXPECT_CALL(delegate_, Stop);
  GetDialogDelegate()->AcceptDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.FailedInteraction",
      PasswordChangeDialogAction::kAcceptButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       ErrorDialogCancelled) {
  UpdateState(PasswordChangeDelegate::State::kPasswordChangeFailed);

  EXPECT_CALL(delegate_, Stop);
  GetDialogDelegate()->CancelDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.FailedInteraction",
      PasswordChangeDialogAction::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       OtpDetectedDialogAccepted) {
  UpdateState(PasswordChangeDelegate::State::kOtpDetected);

  EXPECT_CALL(delegate_, OpenPasswordChangeTab);
  EXPECT_CALL(delegate_, Stop);
  GetDialogDelegate()->AcceptDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.OTPRequested",
      PasswordChangeDialogAction::kAcceptButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       OtpDetectedDialogCancelled) {
  UpdateState(PasswordChangeDelegate::State::kOtpDetected);

  EXPECT_CALL(delegate_, Stop);
  GetDialogDelegate()->CancelDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.OTPRequested",
      PasswordChangeDialogAction::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       CheckingSignInToastShown) {
  UpdateState(PasswordChangeDelegate::State::kWaitingForChangePasswordForm);
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.CheckingSignInToast",
      PasswordChangeToastEvent::kShown,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       ChangingPasswordToastShown) {
  UpdateState(PasswordChangeDelegate::State::kChangingPassword);
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.ChangingPasswordToast",
      PasswordChangeToastEvent::kShown,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       CheckingSignInToastShownAndCancelled) {
  UpdateState(PasswordChangeDelegate::State::kWaitingForChangePasswordForm);

  EXPECT_CALL(delegate_, CancelPasswordChangeFlow);
  views::test::ButtonTestApi clicker(GetToastCloseButton());
  clicker.NotifyClick(ui::test::TestEvent());

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "PasswordManager.PasswordChange.CheckingSignInToast"),
              ElementsAre(Bucket(PasswordChangeToastEvent::kShown, 1),
                          Bucket(PasswordChangeToastEvent::kCanceled, 1)));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       ChangingPasswordToastShownAndCancelled) {
  UpdateState(PasswordChangeDelegate::State::kChangingPassword);

  EXPECT_CALL(delegate_, CancelPasswordChangeFlow);
  views::test::ButtonTestApi clicker(GetToastCloseButton());
  clicker.NotifyClick(ui::test::TestEvent());

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "PasswordManager.PasswordChange.ChangingPasswordToast"),
              ElementsAre(Bucket(PasswordChangeToastEvent::kShown, 1),
                          Bucket(PasswordChangeToastEvent::kCanceled, 1)));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       WaitingForSignInToastShownAndCancelled) {
  UpdateState(PasswordChangeDelegate::State::kLoginFormDetected);

  EXPECT_CALL(delegate_, CancelPasswordChangeFlow);
  views::test::ButtonTestApi clicker(GetToastCloseButton());
  clicker.NotifyClick(ui::test::TestEvent());

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "PasswordManager.PasswordChange.WaitingForUserSignInToast"),
              ElementsAre(Bucket(PasswordChangeToastEvent::kShown, 1),
                          Bucket(PasswordChangeToastEvent::kCanceled, 1)));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       WaitingForSignInToastClickedContinue) {
  UpdateState(PasswordChangeDelegate::State::kLoginFormDetected);

  EXPECT_CALL(delegate_, RetryLoginCheck);
  views::test::ButtonTestApi clicker(GetToastActionButton());
  clicker.NotifyClick(ui::test::TestEvent());

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "PasswordManager.PasswordChange.WaitingForUserSignInToast"),
              ElementsAre(Bucket(PasswordChangeToastEvent::kShown, 1),
                          Bucket(PasswordChangeToastEvent::kRetry, 1)));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       PasswordChangeCanceledToastShownAndAccepted) {
  UpdateState(PasswordChangeDelegate::State::kCanceled);

  EXPECT_CALL(delegate_, OpenPasswordChangeTab);
  EXPECT_CALL(delegate_, Stop);
  views::test::ButtonTestApi clicker(GetToastActionButton());
  clicker.NotifyClick(ui::test::TestEvent());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "PasswordManager.PasswordChange.CanceledToast"),
      ElementsAre(Bucket(PasswordChangeToastEvent::kShown, 1),
                  Bucket(PasswordChangeToastEvent::kOpenPasswordChangeTab, 1)));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       PasswordChangeCanceledToastShownAndClosed) {
  UpdateState(PasswordChangeDelegate::State::kCanceled);

  EXPECT_CALL(delegate_, Stop);
  views::test::ButtonTestApi clicker(GetToastCloseButton());
  clicker.NotifyClick(ui::test::TestEvent());

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "PasswordManager.PasswordChange.CanceledToast"),
              ElementsAre(Bucket(PasswordChangeToastEvent::kShown, 1),
                          Bucket(PasswordChangeToastEvent::kCanceled, 1)));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       PasswordChangeSuccessfilToastShownAndAccepted) {
  UpdateState(PasswordChangeDelegate::State::kPasswordSuccessfullyChanged);

  EXPECT_CALL(delegate_, OpenPasswordDetails);
  EXPECT_CALL(delegate_, Stop);
  views::test::ButtonTestApi clicker(GetToastActionButton());
  clicker.NotifyClick(ui::test::TestEvent());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       PasswordChangeSuccessfilToastShownAndCanceled) {
  UpdateState(PasswordChangeDelegate::State::kPasswordSuccessfullyChanged);

  EXPECT_CALL(delegate_, Stop);
  views::test::ButtonTestApi clicker(GetToastCloseButton());
  clicker.NotifyClick(ui::test::TestEvent());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       ToastDisappearsWhenDialogIsShown) {
  UpdateState(PasswordChangeDelegate::State::kWaitingForChangePasswordForm);

  EXPECT_TRUE(ui_controller()->toast_view());
  EXPECT_FALSE(ui_controller()->dialog_widget());

  UpdateState(PasswordChangeDelegate::State::kChangePasswordFormNotFound);

  EXPECT_FALSE(ui_controller()->toast_view());
  EXPECT_TRUE(ui_controller()->dialog_widget());
}

}  // namespace
