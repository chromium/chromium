// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/importer/import_lock_dialog_view.h"

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class ImportLockDialogViewBrowserTest : public DialogBrowserTest {
 public:
  ImportLockDialogViewBrowserTest() {}

  ImportLockDialogViewBrowserTest(const ImportLockDialogViewBrowserTest&) =
      delete;
  ImportLockDialogViewBrowserTest& operator=(
      const ImportLockDialogViewBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
    ImportLockDialogView::Show(native_window, base::OnceCallback<void(bool)>());
  }
};

// Invokes a dialog that implores the user to close Firefox before trying to
// import data.
IN_PROC_BROWSER_TEST_F(ImportLockDialogViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
