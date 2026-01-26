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
constexpr char kScreenshotBaselineCL[] = "7206051";

class DefaultBrowserModalPixelTest : public InteractiveBrowserTest {
 public:
  DefaultBrowserModalPixelTest() = default;
  ~DefaultBrowserModalPixelTest() override = default;

  void ShowUi() {
    DefaultBrowserModalDialog::Show(
        browser()->profile(),
        browser()->tab_strip_model()->GetActiveWebContents()->GetNativeView());
  }
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserModalPixelTest, ShowAndVerifyUi) {
  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      Do([this]() { ShowUi(); }),
      InAnyContext(
          WaitForShow(DefaultBrowserModalDialog::kDefaultBrowserModalDialogId)),
      InSameContext(ScreenshotSurface(
          DefaultBrowserModalDialog::kDefaultBrowserModalDialogId,
          /*screenshot_name=*/"DefaultBrowserModal", kScreenshotBaselineCL)));
}

}  // namespace default_browser
