// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_change_ui_controller.h"

#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/password_change/features.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/password_change_ui_delegate_mock.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/test_event.h"
#include "ui/lottie/animation.h"
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
    host_resolver()->AddRule("*", "127.0.0.1");
    tabs::TabInterface* tab_interface = browser()->GetActiveTabInterface();
    ASSERT_TRUE(tab_interface);
    ui_controller_ =
        std::make_unique<PasswordChangeUIController>(&delegate_, tab_interface);
  }

  void TearDownOnMainThread() override {
    // Needed to avoid dangling pointer to tab interface.
    ui_controller_ = nullptr;
  }

  void UpdateState(PasswordChangeUIDelegate::State state) {
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
  PasswordChangeUIDelegateMock delegate_;
  std::unique_ptr<PasswordChangeUIController> ui_controller_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       OfferingPasswordChangeDialogAccepted) {
  UpdateState(PasswordChangeUIDelegate::State::kOfferingPasswordChange);

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
  UpdateState(PasswordChangeUIDelegate::State::kOfferingPasswordChange);

  EXPECT_EQ(GetDialogDelegate()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_TITLE));

  EXPECT_CALL(delegate_, OnPasswordChangeDeclined);
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
  UpdateState(PasswordChangeUIDelegate::State::kWaitingForAgreement);

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
  UpdateState(PasswordChangeUIDelegate::State::kWaitingForAgreement);

  EXPECT_EQ(GetDialogDelegate()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_TITLE));
  EXPECT_CALL(delegate_, OnPasswordChangeDeclined);
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
  UpdateState(PasswordChangeUIDelegate::State::kChangePasswordFormNotFound);

  EXPECT_CALL(delegate_, OpenPasswordChangeTab);
  GetDialogDelegate()->AcceptDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.NoPasswordForm",
      PasswordChangeDialogAction::kAcceptButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       PasswordFormNotFoundDialogCancelled) {
  UpdateState(PasswordChangeUIDelegate::State::kChangePasswordFormNotFound);

  EXPECT_CALL(delegate_, OnUiDismissed);
  GetDialogDelegate()->CancelDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.NoPasswordForm",
      PasswordChangeDialogAction::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       ErrorDialogAccepted) {
  UpdateState(PasswordChangeUIDelegate::State::kPasswordChangeFailed);

  EXPECT_CALL(delegate_, OpenPasswordChangeTab);
  GetDialogDelegate()->AcceptDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.FailedInteraction",
      PasswordChangeDialogAction::kAcceptButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       ErrorDialogCancelled) {
  UpdateState(PasswordChangeUIDelegate::State::kPasswordChangeFailed);

  EXPECT_CALL(delegate_, OnUiDismissed);
  GetDialogDelegate()->CancelDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.FailedInteraction",
      PasswordChangeDialogAction::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       OtpDetectedDialogAccepted) {
  UpdateState(PasswordChangeUIDelegate::State::kOtpDetected);

  EXPECT_CALL(delegate_, OpenPasswordChangeTab);
  GetDialogDelegate()->AcceptDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.OTPRequested",
      PasswordChangeDialogAction::kAcceptButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       OtpDetectedDialogCancelled) {
  UpdateState(PasswordChangeUIDelegate::State::kOtpDetected);

  EXPECT_CALL(delegate_, OnUiDismissed);
  GetDialogDelegate()->CancelDialog();

  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.OTPRequested",
      PasswordChangeDialogAction::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       CheckingSignInToastShown) {
  UpdateState(PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm);
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.CheckingSignInToast",
      PasswordChangeToastEvent::kShown,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       ChangingPasswordToastShown) {
  UpdateState(PasswordChangeUIDelegate::State::kChangingPassword);
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.PasswordChange.ChangingPasswordToast",
      PasswordChangeToastEvent::kShown,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       CheckingSignInToastShownAndCancelled) {
  UpdateState(PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm);

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
  UpdateState(PasswordChangeUIDelegate::State::kChangingPassword);

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
  UpdateState(PasswordChangeUIDelegate::State::kLoginFormDetected);

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
  UpdateState(PasswordChangeUIDelegate::State::kLoginFormDetected);

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
  UpdateState(PasswordChangeUIDelegate::State::kCanceled);

  EXPECT_CALL(delegate_, OpenPasswordChangeTab);
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
  UpdateState(PasswordChangeUIDelegate::State::kCanceled);

  EXPECT_CALL(delegate_, OnUiDismissed);
  views::test::ButtonTestApi clicker(GetToastCloseButton());
  clicker.NotifyClick(ui::test::TestEvent());

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "PasswordManager.PasswordChange.CanceledToast"),
              ElementsAre(Bucket(PasswordChangeToastEvent::kShown, 1),
                          Bucket(PasswordChangeToastEvent::kCanceled, 1)));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       PasswordChangeSuccessfulToastShownAndAccepted) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("example.com", "/empty.html")));

  const GURL change_password_url("https://example.com/change-password");
  const std::u16string username = u"user@example.com";
  const std::u16string new_password = u"NewSecurePassword123";
  EXPECT_CALL(delegate_, GetChangePasswordUrl)
      .WillOnce(testing::ReturnRef(change_password_url));
  EXPECT_CALL(delegate_, GetUsername).WillOnce(testing::ReturnRef(username));
  EXPECT_CALL(delegate_, GetGeneratedPassword)
      .WillOnce(testing::Return(
          password_manager::PasswordString(std::u16string(new_password))));
  EXPECT_CALL(delegate_, OnUiDismissed);

  UpdateState(PasswordChangeUIDelegate::State::kPasswordSuccessfullyChanged);

  views::test::ButtonTestApi clicker(GetToastActionButton());
  clicker.NotifyClick(ui::test::TestEvent());

  auto* passwords_ui_controller = ManagePasswordsUIController::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(passwords_ui_controller->GetState(),
            password_manager::ui::PASSWORD_CHANGE_STATE);
  EXPECT_EQ(passwords_ui_controller->PasswordChangeUsername(), username);
  EXPECT_EQ(passwords_ui_controller->PasswordChangeNewPassword(), new_password);
}

IN_PROC_BROWSER_TEST_F(
    PasswordChangeUIControllerBrowserTest,
    PasswordChangeSuccessfulToastShownAndAccepted_CrossDomainNavigatesToSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("other.com", "/empty.html")));

  const GURL change_password_url("https://example.com/change-password");
  EXPECT_CALL(delegate_, GetChangePasswordUrl)
      .WillOnce(testing::ReturnRef(change_password_url));
  EXPECT_CALL(delegate_, GetDisplayOrigin)
      .WillOnce(testing::Return(u"example.com"));
  EXPECT_CALL(delegate_, OnUiDismissed);

  UpdateState(PasswordChangeUIDelegate::State::kPasswordSuccessfullyChanged);

  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  views::test::ButtonTestApi clicker(GetToastActionButton());
  clicker.NotifyClick(ui::test::TestEvent());
  tab_waiter.Wait();

  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL("chrome://password-manager/passwords/example.com"));
}

class PasswordChangeUIControllerWithShowTabOnSuccessBrowserTest
    : public PasswordChangeUIControllerBrowserTest {
 public:
  PasswordChangeUIControllerWithShowTabOnSuccessBrowserTest() {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kShowTabWithPasswordChangeOnSuccess);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PasswordChangeUIControllerWithShowTabOnSuccessBrowserTest,
    PasswordChangeSuccessfulToastShownAndAccepted) {
  UpdateState(PasswordChangeUIDelegate::State::kPasswordSuccessfullyChanged);

  EXPECT_CALL(delegate_, OpenPasswordChangeTab);
  views::test::ButtonTestApi clicker(GetToastActionButton());
  clicker.NotifyClick(ui::test::TestEvent());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       PasswordChangeSuccessfulToastShownAndCanceled) {
  UpdateState(PasswordChangeUIDelegate::State::kPasswordSuccessfullyChanged);

  EXPECT_CALL(delegate_, OnUiDismissed);
  views::test::ButtonTestApi clicker(GetToastCloseButton());
  clicker.NotifyClick(ui::test::TestEvent());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUIControllerBrowserTest,
                       ToastDisappearsWhenDialogIsShown) {
  UpdateState(PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm);

  EXPECT_TRUE(ui_controller()->toast_view());
  EXPECT_FALSE(ui_controller()->dialog_widget());

  UpdateState(PasswordChangeUIDelegate::State::kChangePasswordFormNotFound);

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
  UpdateState(PasswordChangeUIDelegate::State::kChangingPassword);

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

  UpdateState(PasswordChangeUIDelegate::State::kWaitingForAgreement);

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

  UpdateState(PasswordChangeUIDelegate::State::kWaitingForAgreement);

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

  UpdateState(PasswordChangeUIDelegate::State::kOfferingPasswordChange);

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

  UpdateState(PasswordChangeUIDelegate::State::kOfferingPasswordChange);

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

  UpdateState(PasswordChangeUIDelegate::State::kWaitingForAgreement);

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

  UpdateState(PasswordChangeUIDelegate::State::kWaitingForAgreement);

  views::BubbleFrameView* frame_view =
      GetDialogDelegate()->AsBubbleDialogDelegate()->GetBubbleFrameView();
  ASSERT_TRUE(frame_view);
  auto* animated_view =
      views::AsViewClass<views::ThemeTrackingAnimatedImageView>(
          frame_view->GetHeaderViewForTesting());
  ASSERT_TRUE(animated_view);
  ASSERT_TRUE(animated_view->animated_image());
  ASSERT_TRUE(animated_view->animated_image()->GetPlaybackConfig());
  EXPECT_EQ(animated_view->animated_image()->GetPlaybackConfig()->style,
            lottie::Animation::Style::kLinear);
}

IN_PROC_BROWSER_TEST_F(
    PasswordChangeUIControllerWithPrivateInferenceBrowserTest,
    OfferingPasswordChangeAnimatedBannerSetAsHeaderView) {
  EXPECT_CALL(delegate_, GetDisplayOrigin)
      .WillRepeatedly(testing::Return(u"example.com"));

  UpdateState(PasswordChangeUIDelegate::State::kOfferingPasswordChange);

  views::BubbleFrameView* frame_view =
      GetDialogDelegate()->AsBubbleDialogDelegate()->GetBubbleFrameView();
  ASSERT_TRUE(frame_view);
  auto* animated_view =
      views::AsViewClass<views::ThemeTrackingAnimatedImageView>(
          frame_view->GetHeaderViewForTesting());
  ASSERT_TRUE(animated_view);
  ASSERT_TRUE(animated_view->animated_image());
  ASSERT_TRUE(animated_view->animated_image()->GetPlaybackConfig());
  EXPECT_EQ(animated_view->animated_image()->GetPlaybackConfig()->style,
            lottie::Animation::Style::kLinear);
}

}  // namespace
