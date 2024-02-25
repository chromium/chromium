// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

namespace {

class DocumentScanDiscoveryConfirmationDialogTest : public DialogBrowserTest {
 public:
  DocumentScanDiscoveryConfirmationDialogTest() = default;

  DocumentScanDiscoveryConfirmationDialogTest(
      const DocumentScanDiscoveryConfirmationDialogTest&) = delete;
  DocumentScanDiscoveryConfirmationDialogTest& operator=(
      const DocumentScanDiscoveryConfirmationDialogTest&) = delete;

  void ShowUi(const std::string& name) override {
    extensions::ShowDocumentScannerDiscoveryConfirmationDialog(
        browser()->window()->GetNativeWindow(),
        "DocumentScanDiscoveryConfirmationDialogTest", u"Extension Name", {},
        base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(DocumentScanDiscoveryConfirmationDialogTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

}  // namespace
