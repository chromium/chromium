// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/extensions/request_file_system_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helper class to display the RequestFileSystemDialogView dialog for testing.
class RequestFileSystemDialogTest : public DialogBrowserTest {
 public:
  RequestFileSystemDialogTest() {}

  void ShowUi(const std::string& name) override {
    RequestFileSystemDialogView::ShowDialog(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "RequestFileSystemDialogTest", "TestVolume", true,
        base::BindOnce(&RequestFileSystemDialogTest::DialogCallback));
  }

 private:
  static void DialogCallback(ui::DialogButton button) {}

  DISALLOW_COPY_AND_ASSIGN(RequestFileSystemDialogTest);
};

IN_PROC_BROWSER_TEST_F(RequestFileSystemDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

}  // namespace
