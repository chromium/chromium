// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eula_dialog_linux.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class EulaDialogLinuxBrowserTest : public DialogBrowserTest {
 public:
  EulaDialogLinuxBrowserTest() = default;
  EulaDialogLinuxBrowserTest(const EulaDialogLinuxBrowserTest&) = delete;
  EulaDialogLinuxBrowserTest& operator=(const EulaDialogLinuxBrowserTest&) =
      delete;
  ~EulaDialogLinuxBrowserTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    EulaDialog::Show(base::DoNothing());
  }
};

// TODO(crbug.com/470084686): Disabled on Linux ASan from
// linux.asan.browser_tests.filter.
IN_PROC_BROWSER_TEST_F(EulaDialogLinuxBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
