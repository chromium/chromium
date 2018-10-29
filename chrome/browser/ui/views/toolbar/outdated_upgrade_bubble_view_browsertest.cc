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

 private:
  DISALLOW_COPY_AND_ASSIGN(OutdatedUpgradeBubbleTest);
};

#if defined(OS_MACOSX)
// This bubble doesn't show on Mac right now: https://crbug.com/764111
#define MAYBE_InvokeUi_Outdated DISABLED_InvokeUi_Outdated
#else
#define MAYBE_InvokeUi_Outdated InvokeUi_Outdated
#endif
IN_PROC_BROWSER_TEST_F(OutdatedUpgradeBubbleTest, MAYBE_InvokeUi_Outdated) {
  ShowAndVerifyUi();
}

#if defined(OS_MACOSX)
// This bubble doesn't show on Mac right now: https://crbug.com/764111
#define MAYBE_InvokeUi_NoAutoUpdate DISABLED_InvokeUi_NoAutoUpdate
#else
#define MAYBE_InvokeUi_NoAutoUpdate InvokeUi_NoAutoUpdate
#endif
IN_PROC_BROWSER_TEST_F(OutdatedUpgradeBubbleTest, MAYBE_InvokeUi_NoAutoUpdate) {
  ShowAndVerifyUi();
}

// The critical upgrade dialog is intentionally only shown on Windows.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(OutdatedUpgradeBubbleTest, InvokeUi_Critical) {
  ShowAndVerifyUi();
}
#endif
