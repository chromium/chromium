// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"

#include <tuple>

#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome_signout_confirmation_prompt.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/any_widget_observer.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/path_service.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/signin_test_util.h"
#include "chrome/browser/extensions/sync/account_extension_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "extensions/common/extension.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class ChromeSignoutConfirmationPromptPixelTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<
          std::tuple<ChromeSignoutConfirmationPromptVariant, size_t>> {
 public:
  ChromeSignoutConfirmationPromptPixelTest() {
    feature_list_.InitAndEnableFeature(syncer::kUnoPhase2FollowUp);
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

    auto* controller = browser()->GetFeatures().signin_view_controller();
    controller->ShowSignoutConfirmationPrompt(
        GetVariant(), GetUnsyncedDataCount(),
        base::BindOnce([](ChromeSignoutConfirmationChoice choice,
                          bool uninstall_extensions) {}));

    widget_waiter.WaitIfNeededAndGet();
    observer.Wait();
  }

  static std::string GetTestSuffix(
      const testing::TestParamInfo<
          ChromeSignoutConfirmationPromptPixelTest::ParamType>& info) {
    ChromeSignoutConfirmationPromptVariant variant = std::get<0>(info.param);
    size_t unsynced_data_count = std::get<1>(info.param);
    switch (variant) {
      case ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData:
        return "NoUnsyncedData";
      case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
        return base::StringPrintf("UnsyncedData_%zu_UnsyncedItems",
                                  unsynced_data_count);
      case ChromeSignoutConfirmationPromptVariant::
          kUnsyncedDataWithReauthButton:
        return base::StringPrintf(
            "UnsyncedDataWithReauthButton_%zu_UnsyncedItems",
            unsynced_data_count);
      case ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls:
        return "SupervisedProfile";
    }
  }

 protected:
  ChromeSignoutConfirmationPromptVariant GetVariant() const {
    return std::get<0>(GetParam());
  }

  size_t GetUnsyncedDataCount() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ChromeSignoutConfirmationPromptPixelTest,
                       InvokeUi_Default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ChromeSignoutConfirmationPromptPixelTest,
    testing::Values(
        std::make_tuple(ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData,
                        0U),
        std::make_tuple(ChromeSignoutConfirmationPromptVariant::kUnsyncedData,
                        1U),
        std::make_tuple(ChromeSignoutConfirmationPromptVariant::kUnsyncedData,
                        2U),
        std::make_tuple(ChromeSignoutConfirmationPromptVariant::
                            kUnsyncedDataWithReauthButton,
                        1U),
        std::make_tuple(ChromeSignoutConfirmationPromptVariant::
                            kUnsyncedDataWithReauthButton,
                        2U),
        std::make_tuple(ChromeSignoutConfirmationPromptVariant::
                            kProfileWithParentalControls,
                        0U)),
    &ChromeSignoutConfirmationPromptPixelTest::GetTestSuffix);

#if BUILDFLAG(ENABLE_EXTENSIONS)

// Same as ChromeSignoutConfirmationPromptPixelTest except an account extension
// is installed before the dialog is shown.
class ChromeSignoutConfirmationPromptWithExtensionsPixelTest
    : public SigninBrowserTestBaseT<ChromeSignoutConfirmationPromptPixelTest> {
 public:
  ChromeSignoutConfirmationPromptWithExtensionsPixelTest() {
    base::FilePath test_data_dir;
    if (!base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir)) {
      ADD_FAILURE();
      return;
    }
    extension_data_dir_ = test_data_dir.AppendASCII("extensions");
  }

 protected:
  base::FilePath extension_data_dir() { return extension_data_dir_; }

 private:
  // chrome/test/data/extensions/
  base::FilePath extension_data_dir_;
};

IN_PROC_BROWSER_TEST_P(ChromeSignoutConfirmationPromptWithExtensionsPixelTest,
                       InvokeUi_Default) {
  extensions::signin_test_util::SimulateExplicitSignIn(browser()->profile(),
                                                       identity_test_env());

  // Install an account extension before showing the dialog.
  extensions::ChromeTestExtensionLoader extension_loader(browser()->profile());
  extension_loader.set_pack_extension(true);
  scoped_refptr<const extensions::Extension> account_extension =
      extension_loader.LoadExtension(
          extension_data_dir().AppendASCII("simple_with_icon"));

  extensions::AccountExtensionTracker::Get(browser()->profile())
      ->SetAccountExtensionTypeForTesting(
          account_extension->id(),
          extensions::AccountExtensionTracker::AccountExtensionType::
              kAccountInstalledSignedIn);

  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ChromeSignoutConfirmationPromptWithExtensionsPixelTest,
    testing::Values(
        std::make_tuple(ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData,
                        0U),
        std::make_tuple(ChromeSignoutConfirmationPromptVariant::kUnsyncedData,
                        1U),
        std::make_tuple(ChromeSignoutConfirmationPromptVariant::kUnsyncedData,
                        2U),
        std::make_tuple(ChromeSignoutConfirmationPromptVariant::
                            kUnsyncedDataWithReauthButton,
                        1U),
        std::make_tuple(ChromeSignoutConfirmationPromptVariant::
                            kUnsyncedDataWithReauthButton,
                        2U),
        std::make_tuple(ChromeSignoutConfirmationPromptVariant::
                            kProfileWithParentalControls,
                        0U)),
    &ChromeSignoutConfirmationPromptPixelTest::GetTestSuffix);

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
