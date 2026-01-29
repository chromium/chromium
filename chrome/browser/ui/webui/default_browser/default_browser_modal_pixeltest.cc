// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal_dialog_delegate.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace default_browser {

// Baseline Gerrit CL number of the most recent CL that modified the UI.
constexpr char kScreenshotBaselineCL[] = "7511657";

class DefaultBrowserModalPixelTest : public InteractiveBrowserTest {
 public:
  DefaultBrowserModalPixelTest() = default;
  ~DefaultBrowserModalPixelTest() override = default;

  void ShowUi(bool use_settings_illustration) {
    DefaultBrowserModalDialog::Show(
        browser()->profile(),
        browser()->tab_strip_model()->GetActiveWebContents()->GetNativeView(),
        use_settings_illustration);
  }
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserModalPixelTest,
                       ShowAndVerifyUiWithoutSettingsIllustration) {
  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      Do([this]() { ShowUi(/*use_settings_illustration=*/false); }),
      InAnyContext(
          WaitForShow(DefaultBrowserModalDialog::kDefaultBrowserModalDialogId)),
      InSameContext(ScreenshotSurface(
          DefaultBrowserModalDialog::kDefaultBrowserModalDialogId,
          /*screenshot_name=*/"DefaultBrowserModalWithoutSettingsIllustration",
          kScreenshotBaselineCL)));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserModalPixelTest,
                       ShowAndVerifyUiWithSettingsIllustration) {
  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      Do([this]() { ShowUi(/*use_settings_illustration=*/true); }),
      InAnyContext(
          WaitForShow(DefaultBrowserModalDialog::kDefaultBrowserModalDialogId)),
      InSameContext(ScreenshotSurface(
          DefaultBrowserModalDialog::kDefaultBrowserModalDialogId,
          /*screenshot_name=*/"DefaultBrowserModalWithSettingsIllustration",
          kScreenshotBaselineCL)));
}

}  // namespace default_browser
