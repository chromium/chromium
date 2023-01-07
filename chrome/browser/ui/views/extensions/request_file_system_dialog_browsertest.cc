// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/extensions/request_file_system_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_types.h"

namespace {

// Helper class to display the RequestFileSystemDialogView dialog for testing.
class RequestFileSystemDialogTest : public DialogBrowserTest {
 public:
  RequestFileSystemDialogTest() {}

  RequestFileSystemDialogTest(const RequestFileSystemDialogTest&) = delete;
  RequestFileSystemDialogTest& operator=(const RequestFileSystemDialogTest&) =
      delete;

  void ShowUi(const std::string& name) override {
    RequestFileSystemDialogView::ShowDialog(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "RequestFileSystemDialogTest", "TestVolume", true,
        base::BindOnce(&RequestFileSystemDialogTest::DialogCallback,
                       base::Unretained(this)));
  }

  bool did_run_callback() const { return did_run_callback_; }

 private:
  void DialogCallback(ui::DialogButton button) {
    // Assume that in tests, this dialog gets canceled, closed, or dismissed.
    // As a result, callback is treated as cancel
    EXPECT_EQ(ui::DIALOG_BUTTON_CANCEL, button);
    did_run_callback_ = true;
  }

  bool did_run_callback_ = false;
};

IN_PROC_BROWSER_TEST_F(RequestFileSystemDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
  ASSERT_TRUE(did_run_callback());
}

}  // namespace
