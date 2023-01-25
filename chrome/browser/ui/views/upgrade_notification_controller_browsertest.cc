// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/upgrade_notification_controller.h"

#include "build/build_config.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/test/browser_test.h"

class OutdatedUpgradeBubbleTest : public DialogBrowserTest {
 public:
  OutdatedUpgradeBubbleTest() = default;
  OutdatedUpgradeBubbleTest(const OutdatedUpgradeBubbleTest&) = delete;
  OutdatedUpgradeBubbleTest& operator=(const OutdatedUpgradeBubbleTest&) =
      delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto* const upgrade_notification_controller =
        UpgradeNotificationController::FromBrowser(browser());
    CHECK(upgrade_notification_controller);

    if (name == "Outdated") {
      upgrade_notification_controller->OnOutdatedInstall();
    } else if (name == "NoAutoUpdate") {
      upgrade_notification_controller->OnOutdatedInstallNoAutoUpdate();
    } else if (name == "Critical") {
      upgrade_notification_controller->OnCriticalUpgradeInstalled();
    } else {
      ADD_FAILURE();
    }
  }
};

IN_PROC_BROWSER_TEST_F(OutdatedUpgradeBubbleTest, InvokeUi_Outdated) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(OutdatedUpgradeBubbleTest, InvokeUi_NoAutoUpdate) {
  ShowAndVerifyUi();
}

// The critical upgrade dialog is intentionally only shown on Windows.
#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(OutdatedUpgradeBubbleTest, InvokeUi_Critical) {
  ShowAndVerifyUi();
}
#endif
