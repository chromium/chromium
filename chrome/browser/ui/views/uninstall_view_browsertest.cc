// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/uninstall_view.h"

#include <string>

#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/uninstall_browser_prompt.h"
#include "content/public/test/browser_test.h"

class UninstallViewBrowserTest : public DialogBrowserTest {
 public:
  UninstallViewBrowserTest() {}

  UninstallViewBrowserTest(const UninstallViewBrowserTest&) = delete;
  UninstallViewBrowserTest& operator=(const UninstallViewBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // UninstallView may need to know whether Chrome is the default browser,
    // which requires IO. Since this is a test, we'll just allow that.
    base::ScopedAllowBlockingForTesting allow_blocking;

    chrome::ShowUninstallBrowserPrompt();

    // The uninstall dialog is intentionally leaked because it assumes Chrome is
    // about to close anyway, so we have to explicitly exit for the test to end.
    // See ShowUninstallBrowserPrompt in uninstall_view.cc.
    exit(0);
  }
};

// Invokes a dialog confirming that the user wants to uninstall Chrome.
// Disabled because the build bots don't click to dismiss the dialog, they just
// wait for it to time out. Unfortunately because we have to explicitly exit
// (see ShowUi above) this approach doesn't work.
IN_PROC_BROWSER_TEST_F(UninstallViewBrowserTest, DISABLED_InvokeUi_default) {
  ShowAndVerifyUi();
}
