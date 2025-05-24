// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/views/widget/any_widget_observer.h"

// Tests for the chrome://history-sync-optin WebUI page.
namespace {

std::string ParamToTestSuffix(
    const testing::TestParamInfo<PixelTestParam>& info) {
  return info.param.test_suffix;
}

const PixelTestParam kDialogTestParams[] = {
    {.test_suffix = "Regular"},
    {.test_suffix = "DarkTheme", .use_dark_theme = true},
    /* TODO(crbug.com/406751006): Until the strings are translatable the RTL
       language does not fully apply. */
    {.test_suffix = "Rtl", .use_right_to_left_language = true},
};
}  // namespace

class HistorySyncOptinUIDialogPixelTest
    : public ProfilesPixelTestBaseT<DialogBrowserTest>,
      public testing::WithParamInterface<PixelTestParam> {
 public:
  HistorySyncOptinUIDialogPixelTest()
      : ProfilesPixelTestBaseT<DialogBrowserTest>(GetParam()) {}

  ~HistorySyncOptinUIDialogPixelTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    DCHECK(browser());

    SignInWithAccount();
    auto target_url = GURL(chrome::kChromeUIHistorySyncOptinURL);
    content::TestNavigationObserver observer(target_url);
    observer.StartWatchingNewWebContents();

    // ShowUi() can sometimes return before the dialog widget is shown because
    // the call to show the latter is asynchronous. Adding
    // NamedWidgetShownWaiter will prevent that from happening.
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "SigninViewControllerDelegateViews");

    auto* controller = browser()->signin_view_controller();
    controller->ShowModalHistorySyncOptInDialog();
    widget_waiter.WaitIfNeededAndGet();
    observer.Wait();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kEnableHistorySyncOptin};
};

IN_PROC_BROWSER_TEST_P(HistorySyncOptinUIDialogPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         HistorySyncOptinUIDialogPixelTest,
                         testing::ValuesIn(kDialogTestParams),
                         &ParamToTestSuffix);
