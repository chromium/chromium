// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_change_ui_controller.h"

#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/password_change/features.h"
#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/test_event.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/theme_tracking_animated_image_view.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace {

using ::base::Bucket;
using ::testing::ElementsAre;

bool ContainsText(const views::View* view, std::u16string_view text) {
  if (!view) {
    return false;
  }
  if (const auto* label = views::AsViewClass<views::Label>(view)) {
    if (label->GetText().find(text) != std::u16string::npos) {
      return true;
    }
  }
  if (const auto* styled_label = views::AsViewClass<views::StyledLabel>(view)) {
    if (styled_label->GetText().find(text) != std::u16string::npos) {
      return true;
    }
  }
  for (const views::View* child : view->children()) {
    if (ContainsText(child, text)) {
      return true;
    }
  }
  return false;
}

void FindStyledLabels(views::View* root,
                      std::vector<views::StyledLabel*>& results) {
  if (auto* label = views::AsViewClass<views::StyledLabel>(root)) {
    results.push_back(label);
  }
  for (views::View* child : root->children()) {
    FindStyledLabels(child, results);
  }
}

class PasswordChangeUIControllerBrowserTest : public InProcessBrowserTest {
 public:
  PasswordChangeUIControllerBrowserTest() {
    feature_list_.InitAndDisableFeature(
        password_change::features::
            kPasswordChangeWithPrivateInferenceLoginCheck);
  }

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

  const views::View* GetDialogContentsView() {
    return ui_controller_->dialog_widget()->GetContentsView();
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       OfferingPasswordChangeDialogAccepted) {
  UpdateState(PasswordChangeDelegate::State::kOfferingPasswordChange);

  EXPECT_EQ(GetDialogDelegate()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_TITLE));

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

  EXPECT_EQ(GetDialogDelegate()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_TITLE));

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

  EXPECT_EQ(GetDialogDelegate()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_TITLE));
  EXPECT_EQ(GetDialogDelegate()->AsBubbleDialogDelegate()->GetSubtitle(), u"");

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

  EXPECT_EQ(GetDialogDelegate()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_TITLE));
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

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       DISABLED_PageIsInteractableWhileToastIsShowing) {
  // Navigate to a simple page with an input field.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<input id='test_input'>")));

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  // Focus the input field.
  EXPECT_TRUE(content::ExecJs(
      web_contents, "document.getElementById('test_input').focus();"));

  // Show the toast.
  UpdateState(PasswordChangeDelegate::State::kChangingPassword);

  // Verify that the input field is still the active element.
  EXPECT_EQ(
      true,
      content::EvalJs(
          web_contents,
          "document.activeElement === document.getElementById('test_input');"));

  // Simulate typing 'a'.
  content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('a'),
                            ui::DomCode::US_A, ui::VKEY_A, false, false, false,
                            false);

  // Verify the value.
  EXPECT_EQ("a",
            content::EvalJs(web_contents,
                            "document.getElementById('test_input').value;"));
}

class PasswordChangeUIControllerWithPrivateInferenceBrowserTest
    : public PasswordChangeUIControllerBrowserTest {
 public:
  PasswordChangeUIControllerWithPrivateInferenceBrowserTest() {
    feature_list_.InitAndEnableFeature(
        password_change::features::
            kPasswordChangeWithPrivateInferenceLoginCheck);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PasswordChangeUIControllerWithPrivateInferenceBrowserTest,
    PrivacyNoticeDialogAccepted) {
  EXPECT_CALL(delegate_, GetDisplayOrigin)
      .WillRepeatedly(testing::Return(u"example.com"));

  UpdateState(PasswordChangeDelegate::State::kWaitingForAgreement);

  EXPECT_EQ(
      GetDialogDelegate()->GetWindowTitle(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_OFFER_DIALOG_TITLE_WITH_PI));
  EXPECT_EQ(GetDialogDelegate()->AsBubbleDialogDelegate()->GetSubtitle(),
            u"example.com");
  EXPECT_EQ(
      GetDialogDelegate()->GetDialogButtonLabel(ui::mojom::DialogButton::kOk),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_TRY_NOW_BUTTON));
  EXPECT_TRUE(ContainsText(
      GetDialogContentsView(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_LINK_WITH_PRIVACY_NOTICE)));
  EXPECT_FALSE(ContainsText(
      GetDialogContentsView(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_LINK_WITHOUT_PRIVACY_NOTICE)));
  EXPECT_TRUE(ContainsText(
      GetDialogContentsView(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_PRIVACY_NOTICE_WITH_PI)));
  EXPECT_TRUE(ContainsText(
      GetDialogContentsView(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_MANAGE_IN_SETTINGS)));

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
  histogram_tester_.ExpectTotalCount(
      "PasswordManager.PasswordChange.LeakDetectionDialog.TimeSpent."
      "WithPrivacyNotice",
      1);
}

IN_PROC_BROWSER_TEST_F(
    PasswordChangeUIControllerWithPrivateInferenceBrowserTest,
    PrivacyNoticeDialogCancelled) {
  EXPECT_CALL(delegate_, GetDisplayOrigin)
      .WillRepeatedly(testing::Return(u"example.com"));

  UpdateState(PasswordChangeDelegate::State::kWaitingForAgreement);

  EXPECT_EQ(
      GetDialogDelegate()->GetWindowTitle(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_OFFER_DIALOG_TITLE_WITH_PI));
  EXPECT_TRUE(ContainsText(
      GetDialogContentsView(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_PRIVACY_NOTICE_WITH_PI)));
  EXPECT_TRUE(ContainsText(
      GetDialogContentsView(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_MANAGE_IN_SETTINGS)));

  EXPECT_CALL(delegate_, OnPasswordChangeDeclined);
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
  histogram_tester_.ExpectTotalCount(
      "PasswordManager.PasswordChange.LeakDetectionDialog.TimeSpent."
      "WithPrivacyNotice",
      1);
}

IN_PROC_BROWSER_TEST_F(
    PasswordChangeUIControllerWithPrivateInferenceBrowserTest,
    OfferingPasswordChangeDialogAccepted) {
  EXPECT_CALL(delegate_, GetDisplayOrigin)
      .WillRepeatedly(testing::Return(u"example.com"));

  UpdateState(PasswordChangeDelegate::State::kOfferingPasswordChange);

  EXPECT_EQ(
      GetDialogDelegate()->GetWindowTitle(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_OFFER_DIALOG_TITLE_WITH_PI));
  EXPECT_EQ(
      GetDialogDelegate()->GetDialogButtonLabel(ui::mojom::DialogButton::kOk),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CHANGE_PASSWORD));
  EXPECT_TRUE(ContainsText(
      GetDialogContentsView(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_LINK_WITHOUT_PRIVACY_NOTICE)));
  EXPECT_FALSE(ContainsText(
      GetDialogContentsView(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_LINK_WITH_PRIVACY_NOTICE)));
  EXPECT_FALSE(ContainsText(
      GetDialogContentsView(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_PRIVACY_NOTICE_WITH_PI)));
  EXPECT_FALSE(ContainsText(
      GetDialogContentsView(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_MANAGE_IN_SETTINGS)));

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
  histogram_tester_.ExpectTotalCount(
      "PasswordManager.PasswordChange.LeakDetectionDialog.TimeSpent."
      "WithoutPrivacyNotice",
      1);
}

IN_PROC_BROWSER_TEST_F(
    PasswordChangeUIControllerWithPrivateInferenceBrowserTest,
    OfferingPasswordChangeDialogCancelled) {
  EXPECT_CALL(delegate_, GetDisplayOrigin)
      .WillRepeatedly(testing::Return(u"example.com"));

  UpdateState(PasswordChangeDelegate::State::kOfferingPasswordChange);

  EXPECT_EQ(
      GetDialogDelegate()->GetWindowTitle(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_OFFER_DIALOG_TITLE_WITH_PI));
  EXPECT_FALSE(ContainsText(
      GetDialogContentsView(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_PRIVACY_NOTICE_WITH_PI)));
  EXPECT_FALSE(ContainsText(
      GetDialogContentsView(),
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_MANAGE_IN_SETTINGS)));

  EXPECT_CALL(delegate_, OnPasswordChangeDeclined);
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
  histogram_tester_.ExpectTotalCount(
      "PasswordManager.PasswordChange.LeakDetectionDialog.TimeSpent."
      "WithoutPrivacyNotice",
      1);
}

IN_PROC_BROWSER_TEST_F(
    PasswordChangeUIControllerWithPrivateInferenceBrowserTest,
    PrivacyNoticeDialogLinkClicked) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<div>Test</div>")));

  EXPECT_CALL(delegate_, GetDisplayOrigin)
      .WillRepeatedly(testing::Return(u"example.com"));

  UpdateState(PasswordChangeDelegate::State::kWaitingForAgreement);

  std::vector<views::StyledLabel*> styled_labels;
  FindStyledLabels(ui_controller_->dialog_widget()->GetContentsView(),
                   styled_labels);
  ASSERT_FALSE(styled_labels.empty());

  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  styled_labels.back()->ClickFirstLinkForTesting();
  tab_waiter.Wait();

  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL(chrome::kChromeUiPasswordChangeUrl));

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.LeakDetectionDialog",
      PasswordChangeDialogAction::kLinkClicked,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.LeakDetectionDialog.WithPrivacyNotice",
      PasswordChangeDialogAction::kLinkClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(
    PasswordChangeUIControllerWithPrivateInferenceBrowserTest,
    AnimatedBannerSetAsHeaderView) {
  EXPECT_CALL(delegate_, GetDisplayOrigin)
      .WillRepeatedly(testing::Return(u"example.com"));

  UpdateState(PasswordChangeDelegate::State::kWaitingForAgreement);

  views::BubbleFrameView* frame_view =
      GetDialogDelegate()->AsBubbleDialogDelegate()->GetBubbleFrameView();
  ASSERT_TRUE(frame_view);
  EXPECT_TRUE(views::IsViewClass<views::ThemeTrackingAnimatedImageView>(
      frame_view->GetHeaderViewForTesting()));
}

IN_PROC_BROWSER_TEST_F(
    PasswordChangeUIControllerWithPrivateInferenceBrowserTest,
    OfferingPasswordChangeAnimatedBannerSetAsHeaderView) {
  EXPECT_CALL(delegate_, GetDisplayOrigin)
      .WillRepeatedly(testing::Return(u"example.com"));

  UpdateState(PasswordChangeDelegate::State::kOfferingPasswordChange);

  views::BubbleFrameView* frame_view =
      GetDialogDelegate()->AsBubbleDialogDelegate()->GetBubbleFrameView();
  ASSERT_TRUE(frame_view);
  EXPECT_TRUE(views::IsViewClass<views::ThemeTrackingAnimatedImageView>(
      frame_view->GetHeaderViewForTesting()));
}

}  // namespace
