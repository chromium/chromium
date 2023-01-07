// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crypto_module_password_dialog_view.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class CryptoModulePasswordDialogTest : public DialogBrowserTest {
 public:
  CryptoModulePasswordDialogTest() = default;
  CryptoModulePasswordDialogTest(const CryptoModulePasswordDialogTest&) =
      delete;
  CryptoModulePasswordDialogTest& operator=(
      const CryptoModulePasswordDialogTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowCryptoModulePasswordDialog(
        "Slot", false, kCryptoModulePasswordListCerts, "hostname",
        browser()->window()->GetNativeWindow(), base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(CryptoModulePasswordDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
