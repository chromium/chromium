// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/policy/core/common/features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {

// Helper function to click buttons in the WebUI.
::testing::AssertionResult WaitForAndClickButton(
    content::WebContents* web_contents,
    const std::string& app,
    const std::string& button_id,
    bool log_on_failure = true) {
  content::WaitForLoadStop(web_contents);
  std::string script = base::StringPrintf(R"(
    new Promise((resolve) => {
      const interval = setInterval(() => {
        const button = document.querySelector('%s')?.shadowRoot?.querySelector('#%s');
        if (button && !button.hidden) {
          clearInterval(interval);
          button.click();
          resolve(true);
        }
      }, 50);
    });
  )",
                                          app.c_str(), button_id.c_str());

  ::testing::AssertionResult result = content::ExecJs(web_contents, script);
  if (!result && log_on_failure) {
    LOG(ERROR) << "WaitForAndClickButton failed for " << app << " -> "
               << button_id << ": " << result.message();
  }
  return result;
}

}  // namespace

class DeviceSignalsDisclaimerInteractiveTest : public SigninBrowserTestBase {
 public:
  DeviceSignalsDisclaimerInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {policy::features::kDeviceSignalsBackfillDisclaimer,
         switches::kEnforceManagementDisclaimer},
        {});
  }

 protected:
  content::WebContents* GetModalDialogWebContents(
      BrowserWindowInterface* browser) {
    return SigninViewController::From(browser)
        ->GetModalDialogWebContentsForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerInteractiveTest,
                       PressEscapeThenClickProceed) {
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "email@example.com", signin::ConsentLevel::kSignin);

  base::test::TestFuture<signin::DeviceSignalsDisclaimerResult> result_future;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  SigninViewController::From(browser())->ShowModalManagedUserNoticeDialog(
      signin::EnterpriseProfileCreationDialogParams::
          CreateForDeviceSignalsDisclaimer(account_info,
                                           result_future.GetCallback(),
                                           /*is_modal_dialog=*/true));

  views::Widget* modal_widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(modal_widget);
  content::WebContents* dialog_contents = GetModalDialogWebContents(browser());
  ASSERT_TRUE(dialog_contents);

  // Pressing Escape should not close the modal dialog.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, /*control=*/false, /*shift=*/false,
      /*alt=*/false, /*command=*/false));
  EXPECT_FALSE(modal_widget->IsClosed());

  // Click proceed button on the still-open dialog.
  ASSERT_TRUE(WaitForAndClickButton(dialog_contents,
                                    "managed-user-profile-notice-app",
                                    "proceed-button", /*log_on_failure=*/true));

  EXPECT_EQ(result_future.Get(),
            signin::DeviceSignalsDisclaimerResult::kAccepted);
}
