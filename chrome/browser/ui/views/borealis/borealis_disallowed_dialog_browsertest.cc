// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ui/views/borealis/borealis_disallowed_dialog.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

namespace views::borealis {

class BorealisDisallowedDialogBrowserTest : public DialogBrowserTest {
 public:
  void ShowUi(const std::string& name) override {
    if (name == "Launcher") {
      ShowLauncherDisallowedDialog(
          ::borealis::BorealisFeatures::AllowStatus::kInsufficientHardware);
    } else if (name == "Installer") {
      ShowInstallerDisallowedDialog(
          ::borealis::BorealisFeatures::AllowStatus::kInsufficientHardware);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
};

IN_PROC_BROWSER_TEST_F(BorealisDisallowedDialogBrowserTest,
                       DisallowedDialog_Launcher) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BorealisDisallowedDialogBrowserTest,
                       DisallowedDialog_Installer) {
  ShowAndVerifyUi();
}

}  // namespace views::borealis
