// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_dangerous_file_dialog_view.h"

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/test/browser_test.h"
#include "ui/base/resource/resource_bundle.h"

using SensitiveEntryResult =
    content::FileSystemAccessPermissionContext::SensitiveEntryResult;

class FileSystemAccessDangerousFileDialogViewTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    widget_ = FileSystemAccessDangerousFileDialogView::ShowDialog(
        kTestOrigin, base::FilePath(FILE_PATH_LITERAL("bar.swf")),
        base::BindLambdaForTesting([&](SensitiveEntryResult result) {
          callback_called_ = true;
          callback_result_ = result;
        }),
        browser()->tab_strip_model()->GetActiveWebContents());
  }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));

  raw_ptr<views::Widget> widget_ = nullptr;

  bool callback_called_ = false;
  SensitiveEntryResult callback_result_ = SensitiveEntryResult::kAllowed;
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessDangerousFileDialogViewTest,
                       AcceptRunsCallback) {
  ShowUi(std::string());
  widget_->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SensitiveEntryResult::kAllowed, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessDangerousFileDialogViewTest,
                       CancelRunsCallback) {
  ShowUi(std::string());
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SensitiveEntryResult::kAbort, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessDangerousFileDialogViewTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
