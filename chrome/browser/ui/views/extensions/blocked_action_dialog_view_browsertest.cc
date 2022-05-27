// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_browsertest.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"

class BlockedActionDialogViewBrowserTest : public ExtensionsDialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto extension = InstallExtension("Extension");
    bool show_checkbox = true;
    extensions::ShowBlockedActionDialog(browser(), extension->id(),
                                        show_checkbox, base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(BlockedActionDialogViewBrowserTest, InvokeUi) {
  ShowAndVerifyUi();
}
