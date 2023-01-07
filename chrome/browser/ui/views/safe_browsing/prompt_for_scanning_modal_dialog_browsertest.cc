// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/prompt_for_scanning_modal_dialog.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class PromptForScanningModalDialogTest : public DialogBrowserTest {
 public:
  PromptForScanningModalDialogTest() = default;
  PromptForScanningModalDialogTest(const PromptForScanningModalDialogTest&) =
      delete;
  PromptForScanningModalDialogTest& operator=(
      const PromptForScanningModalDialogTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    safe_browsing::PromptForScanningModalDialog::ShowForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents(), u"Filename",
        base::DoNothing(), base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(PromptForScanningModalDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
