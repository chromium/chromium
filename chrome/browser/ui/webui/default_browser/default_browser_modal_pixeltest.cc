// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_features.h"
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
constexpr char kScreenshotBaselineCL[] = "7638566";

class DefaultBrowserModalPixelTest : public InteractiveBrowserTest {
 protected:
  DefaultBrowserModalPixelTest() = default;
  ~DefaultBrowserModalPixelTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        default_browser::kDefaultBrowserPromptSurfaces,
        {{"prompt_surface", "modal_dialog_without_settings_illustration"}});

    InteractiveBrowserTest::SetUp();
  }

  void ShowUi(bool use_settings_illustration, bool can_pin_to_taskbar = false) {
    DefaultBrowserModalDialog::Show(browser()->profile(),
                                    browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetTopLevelNativeWindow(),
                                    use_settings_illustration,
                                    can_pin_to_taskbar);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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
