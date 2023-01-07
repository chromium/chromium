// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/commerce_prompt.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

// Test harness for integration tests using NtpDiscountConsentView.
class NtpDiscountConsentDialogViewBrowserTest : public DialogBrowserTest {
 public:
  NtpDiscountConsentDialogViewBrowserTest() = default;

  NtpDiscountConsentDialogViewBrowserTest(
      const NtpDiscountConsentDialogViewBrowserTest&) = delete;
  NtpDiscountConsentDialogViewBrowserTest& operator=(
      const NtpDiscountConsentDialogViewBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    commerce::ShowDiscountConsentPrompt(
        browser(),
        base::BindOnce([](chrome_cart::mojom::ConsentStatus status) {}));
  }
};

// Shows the dialog for bookmarking all tabs. This shows a BookmarkEditorView
// dialog, with a tree view, where a user can rename and select a parent folder.
IN_PROC_BROWSER_TEST_F(NtpDiscountConsentDialogViewBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
