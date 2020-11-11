// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_dialog_view.h"

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class RelaunchRequiredDialogViewDialogTest : public DialogBrowserTest {
 protected:
  RelaunchRequiredDialogViewDialogTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    base::Time deadline = base::Time::Now() + base::TimeDelta::FromDays(3);
    RelaunchRequiredDialogView::Show(browser(), deadline, base::DoNothing());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RelaunchRequiredDialogViewDialogTest);
};

IN_PROC_BROWSER_TEST_F(RelaunchRequiredDialogViewDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
