// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_browsertest.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"

class ExtensionMultipleUninstallDialogBrowserTest
    : public ExtensionsDialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto extension_0 = InstallExtension("Extension0");
    auto extension_1 = InstallExtension("Extension1");
    extensions::ShowExtensionMultipleUninstallDialog(
        browser()->profile(), browser()->window()->GetNativeWindow(),
        {extension_0->id(), extension_1->id()}, base::DoNothing(),
        base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionMultipleUninstallDialogBrowserTest, InvokeUi) {
  ShowAndVerifyUi();
}
