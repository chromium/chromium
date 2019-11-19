// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/outdated_upgrade_bubble_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

class OutdatedUpgradeBubbleTest : public DialogBrowserTest {
 public:
  OutdatedUpgradeBubbleTest() = default;
  OutdatedUpgradeBubbleTest(const OutdatedUpgradeBubbleTest&) = delete;
  OutdatedUpgradeBubbleTest& operator=(const OutdatedUpgradeBubbleTest&) =
      delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ToolbarView* toolbar_view =
        BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
    if (name == "Outdated")
      toolbar_view->OnOutdatedInstall();
    else if (name == "NoAutoUpdate")
      toolbar_view->OnOutdatedInstallNoAutoUpdate();
    else if (name == "Critical")
      toolbar_view->OnCriticalUpgradeInstalled();
    else
      ADD_FAILURE();
  }
};

IN_PROC_BROWSER_TEST_F(OutdatedUpgradeBubbleTest, InvokeUi_Outdated) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(OutdatedUpgradeBubbleTest, InvokeUi_NoAutoUpdate) {
  ShowAndVerifyUi();
}

// The critical upgrade dialog is intentionally only shown on Windows.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(OutdatedUpgradeBubbleTest, InvokeUi_Critical) {
  ShowAndVerifyUi();
}
#endif
