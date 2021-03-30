// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/views/extensions/extension_install_friction_dialog_view.h"

#include "base/callback_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

// Helper class to display the ExtensionInstallFrictionDialogView dialog for
// testing.
class ExtensionInstallFrictionDialogTest : public DialogBrowserTest {
 public:
  ExtensionInstallFrictionDialogTest() = default;

  void ShowUi(const std::string& name) override {
    chrome::ShowExtensionInstallFrictionDialog(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::DoNothing::Once<bool>());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallFrictionDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
