// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_dialog_view.h"

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class RelaunchRequiredDialogViewDialogTest : public DialogBrowserTest {
 public:
  RelaunchRequiredDialogViewDialogTest(
      const RelaunchRequiredDialogViewDialogTest&) = delete;
  RelaunchRequiredDialogViewDialogTest& operator=(
      const RelaunchRequiredDialogViewDialogTest&) = delete;

 protected:
  RelaunchRequiredDialogViewDialogTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    base::Time deadline = base::Time::Now() + base::Days(3);
    RelaunchRequiredDialogView::Show(browser(), deadline, base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(RelaunchRequiredDialogViewDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
