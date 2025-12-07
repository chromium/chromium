// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/reload_page_dialog_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_browsertest.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"

// TODO(crbug.com/424012380): Remove browser dependency and enable test for
// Desktop Android
class ReloadPageDialogControllerBrowserTest
    : public ExtensionsDialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto extension = InstallExtension("Extension");

    std::unique_ptr<extensions::ReloadPageDialogController> reload_page_dialog =
        std::make_unique<extensions::ReloadPageDialogController>(
            browser()->tab_strip_model()->GetActiveWebContents(), GetProfile());
    std::vector<const extensions::Extension*> extensions = {extension.get()};
    reload_page_dialog->TriggerShow(extensions);
  }
};

IN_PROC_BROWSER_TEST_F(ReloadPageDialogControllerBrowserTest, InvokeUi) {
  ShowAndVerifyUi();
}
