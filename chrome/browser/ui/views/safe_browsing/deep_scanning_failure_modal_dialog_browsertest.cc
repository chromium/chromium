// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/deep_scanning_failure_modal_dialog.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class DeepScanningFailureModalDialogTest : public DialogBrowserTest {
 public:
  DeepScanningFailureModalDialogTest() = default;
  DeepScanningFailureModalDialogTest(
      const DeepScanningFailureModalDialogTest&) = delete;
  DeepScanningFailureModalDialogTest& operator=(
      const DeepScanningFailureModalDialogTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    safe_browsing::DeepScanningFailureModalDialog::ShowForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents(), base::DoNothing(),
        base::DoNothing(), base::DoNothing(), base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(DeepScanningFailureModalDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
