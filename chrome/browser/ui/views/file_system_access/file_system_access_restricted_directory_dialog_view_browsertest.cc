// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_restricted_directory_dialog_view.h"

#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/test/browser_test.h"
#include "ui/base/resource/resource_bundle.h"

using SensitiveDirectoryResult =
    content::FileSystemAccessPermissionContext::SensitiveDirectoryResult;

class FileSystemAccessRestrictedDirectoryDialogViewTest
    : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    widget_ = FileSystemAccessRestrictedDirectoryDialogView::ShowDialog(
        kTestOrigin, base::FilePath(FILE_PATH_LITERAL("/foo/bar")),
        content::FileSystemAccessPermissionContext::HandleType::kDirectory,
        base::BindLambdaForTesting([&](SensitiveDirectoryResult result) {
          callback_called_ = true;
          callback_result_ = result;
        }),
        browser()->tab_strip_model()->GetActiveWebContents());
  }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));

  views::Widget* widget_ = nullptr;

  bool callback_called_ = false;
  SensitiveDirectoryResult callback_result_ =
      SensitiveDirectoryResult::kAllowed;
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessRestrictedDirectoryDialogViewTest,
                       AcceptRunsCallback) {
  ShowUi(std::string());
  widget_->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SensitiveDirectoryResult::kTryAgain, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessRestrictedDirectoryDialogViewTest,
                       CancelRunsCallback) {
  ShowUi(std::string());
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SensitiveDirectoryResult::kAbort, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessRestrictedDirectoryDialogViewTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
