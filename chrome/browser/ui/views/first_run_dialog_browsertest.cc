// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_dialog.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class FirstRunDialogTest : public DialogBrowserTest {
 public:
  FirstRunDialogTest() = default;
  FirstRunDialogTest(const FirstRunDialogTest&) = delete;
  FirstRunDialogTest& operator=(const FirstRunDialogTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    FirstRunDialog::Show(base::DoNothing(), base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(FirstRunDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
