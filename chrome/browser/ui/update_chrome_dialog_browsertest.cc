// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class UpdateRecommendedDialogTest : public DialogBrowserTest {
 public:
  UpdateRecommendedDialogTest() {}

  UpdateRecommendedDialogTest(const UpdateRecommendedDialogTest&) = delete;
  UpdateRecommendedDialogTest& operator=(const UpdateRecommendedDialogTest&) =
      delete;

  void ShowUi(const std::string& name) override {
    InProcessBrowserTest::browser()->window()->ShowUpdateChromeDialog();
  }
};

// Test that calls ShowUi("default").
IN_PROC_BROWSER_TEST_F(UpdateRecommendedDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
