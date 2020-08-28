// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/invert_bubble_view.h"

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/test/browser_test.h"
#include "ui/native_theme/test_native_theme.h"
#include "ui/views/view.h"

class InvertBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  InvertBubbleViewBrowserTest() = default;
  InvertBubbleViewBrowserTest(const InvertBubbleViewBrowserTest&) = delete;
  InvertBubbleViewBrowserTest& operator=(const InvertBubbleViewBrowserTest&) =
      delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    // TODO(pbos): Check if "set_should_verify_dialog_bounds(false);" can be
    // removed. I changed the below call site so that the dialog gets an anchor.
    set_should_verify_dialog_bounds(false);

    // The InvertBubbleView only triggers in system high-contrast dark mode.
    ui::TestNativeTheme test_theme;
    test_theme.SetIsPlatformHighContrast(true);
    test_theme.SetDarkMode(true);

    BrowserView* const browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    browser_view->SetNativeThemeForTesting(&test_theme);

    MaybeShowInvertBubbleView(browser_view);

    // Remove the test theme before it goes out of scope (this was only used to
    // trigger the now-showing dialog).
    browser_view->SetNativeThemeForTesting(nullptr);
  }
};

// Invokes a bubble that asks the user if they want to install a high contrast
// Chrome theme.
IN_PROC_BROWSER_TEST_F(InvertBubbleViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
