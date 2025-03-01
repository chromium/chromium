// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome_signout_confirmation_prompt.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/any_widget_observer.h"

class ChromeSignoutConfirmationPromptPixelTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<
          ChromeSignoutConfirmationPromptVariant> {
 public:
  ChromeSignoutConfirmationPromptPixelTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop,
                              switches::kImprovedSigninUIOnDesktop},
        /*disabled_features=*/{});
  }

  void ShowUi(const std::string& name) override {
    auto url = GURL(chrome::kChromeUISignoutConfirmationURL);
    content::TestNavigationObserver observer(url);
    observer.StartWatchingNewWebContents();

    // ShowUi() can sometimes return before the dialog widget is shown because
    // the call to show the latter is asynchronous. Adding
    // NamedWidgetShownWaiter will prevent that from happening.
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "SigninViewControllerDelegateViews");

    auto* controller = browser()->signin_view_controller();
    controller->ShowSignoutConfirmationPrompt(
        GetVariant(), base::BindOnce([](ChromeSignoutConfirmationChoice choice,
                                        bool uninstall_extensions) {}));

    widget_waiter.WaitIfNeededAndGet();
    observer.Wait();
  }

  static std::string GetTestSuffix(
      const testing::TestParamInfo<
          ChromeSignoutConfirmationPromptPixelTest::ParamType>& info) {
    switch (info.param) {
      case ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData:
        return "NoUnsyncedData";
      case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
        return "UnsyncedData";
      case ChromeSignoutConfirmationPromptVariant::
          kUnsyncedDataWithReauthButton:
        return "UnsyncedDataWithReauthButton";
      case ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls:
        return "SupervisedProfile";
    }
  }

 protected:
  ChromeSignoutConfirmationPromptVariant GetVariant() const {
    return GetParam();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ChromeSignoutConfirmationPromptPixelTest,
                       InvokeUi_Default) {
  ShowAndVerifyUi();
}

// TODO(crbug.com/399387412): Add a version with an extension installed which
// will add an additional section in the dialog.
INSTANTIATE_TEST_SUITE_P(
    ,
    ChromeSignoutConfirmationPromptPixelTest,
    testing::Values(
        ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData,
        ChromeSignoutConfirmationPromptVariant::kUnsyncedData,
        ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton,
        ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls),
    &ChromeSignoutConfirmationPromptPixelTest::GetTestSuffix);
