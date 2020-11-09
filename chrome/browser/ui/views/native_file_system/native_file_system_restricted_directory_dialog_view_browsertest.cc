// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_restricted_directory_dialog_view.h"

#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/test/browser_test.h"
#include "ui/base/resource/resource_bundle.h"

using SensitiveDirectoryResult =
    content::NativeFileSystemPermissionContext::SensitiveDirectoryResult;

class NativeFileSystemRestrictedDirectoryDialogViewTest
    : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    widget_ = NativeFileSystemRestrictedDirectoryDialogView::ShowDialog(
        kTestOrigin, base::FilePath(FILE_PATH_LITERAL("/foo/bar")),
        content::NativeFileSystemPermissionContext::HandleType::kDirectory,
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

IN_PROC_BROWSER_TEST_F(NativeFileSystemRestrictedDirectoryDialogViewTest,
                       AcceptRunsCallback) {
  ShowUi(std::string());
  widget_->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SensitiveDirectoryResult::kTryAgain, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemRestrictedDirectoryDialogViewTest,
                       CancelRunsCallback) {
  ShowUi(std::string());
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SensitiveDirectoryResult::kAbort, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemRestrictedDirectoryDialogViewTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
