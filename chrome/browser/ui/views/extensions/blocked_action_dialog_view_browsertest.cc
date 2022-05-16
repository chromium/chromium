// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

class BlockedActionDialogViewBrowserTest : public DialogBrowserTest {
 public:
  BlockedActionDialogViewBrowserTest() = default;
  BlockedActionDialogViewBrowserTest(
      const BlockedActionDialogViewBrowserTest&) = delete;
  const BlockedActionDialogViewBrowserTest& operator=(
      const BlockedActionDialogViewBrowserTest&) = delete;
  ~BlockedActionDialogViewBrowserTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto extension = InstallExtension();
    extensions::ShowBlockedActionDialog(browser(), extension->id(),
                                        base::DoNothing());
  }

  scoped_refptr<const extensions::Extension> InstallExtension() {
    scoped_refptr<const extensions::Extension> extension(
        extensions::ExtensionBuilder("Extension").Build());
    extensions::ExtensionSystem::Get(browser()->profile())
        ->extension_service()
        ->AddExtension(extension.get());
    return extension;
  }
};

IN_PROC_BROWSER_TEST_F(BlockedActionDialogViewBrowserTest, InvokeUi) {
  ShowAndVerifyUi();
}
