// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/profiles/device_signals_disclaimer_test_utils.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/policy/core/common/features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {

using ::device_signals_disclaimer_test::GetSelectors;
using ::device_signals_disclaimer_test::ManagedUserProfileNoticeSelectors;
using ::device_signals_disclaimer_test::WaitForAndClickProceedButton;

}  // namespace

// The test parameter indicates whether the refreshed UI is enabled.
class DeviceSignalsDisclaimerInteractiveTest
    : public SigninBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  DeviceSignalsDisclaimerInteractiveTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{policy::features::kDeviceSignalsBackfillDisclaimer, true},
         {switches::kEnforceManagementDisclaimer, true},
         {switches::kFirstRunDesktopRefresh, UseRefreshedUi()},
         {switches::kFirstRunDesktopChoiceScreenRefresh, UseRefreshedUi()},
         {switches::kEnterpriseSigninDialogsRefresh, UseRefreshedUi()}});
  }

 protected:
  bool UseRefreshedUi() const { return GetParam(); }

  const ManagedUserProfileNoticeSelectors& selectors() const {
    return GetSelectors(UseRefreshedUi());
  }

  content::WebContents* GetModalDialogWebContents(
      BrowserWindowInterface* browser) {
    return SigninViewController::From(browser)
        ->GetModalDialogWebContentsForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(DeviceSignalsDisclaimerInteractiveTest,
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
  ASSERT_TRUE(WaitForAndClickProceedButton(dialog_contents, selectors()));

  EXPECT_EQ(result_future.Get(),
            signin::DeviceSignalsDisclaimerResult::kAccepted);
}

INSTANTIATE_TEST_SUITE_P(,
                         DeviceSignalsDisclaimerInteractiveTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Refreshed" : "Legacy";
                         });
