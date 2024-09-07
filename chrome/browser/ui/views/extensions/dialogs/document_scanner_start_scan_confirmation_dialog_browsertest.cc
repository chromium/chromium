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

class DocumentScanStartScanConfirmationDialogTest : public DialogBrowserTest {
 public:
  DocumentScanStartScanConfirmationDialogTest() = default;

  DocumentScanStartScanConfirmationDialogTest(
      const DocumentScanStartScanConfirmationDialogTest&) = delete;
  DocumentScanStartScanConfirmationDialogTest& operator=(
      const DocumentScanStartScanConfirmationDialogTest&) = delete;

  void ShowUi(const std::string& name) override {
    extensions::ShowDocumentScannerStartScanConfirmationDialog(
        browser()->window()->GetNativeWindow(),
        "DocumentScanStartScanConfirmationDialogTest", u"Extension Name",
        u"Scanner Name", {}, base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(DocumentScanStartScanConfirmationDialogTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

}  // namespace
