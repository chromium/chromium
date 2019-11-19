// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_directory_access_confirmation_view.h"

#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/window/dialog_client_view.h"

class NativeFileSystemDirectoryAccessConfirmationViewTest
    : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    widget_ = NativeFileSystemDirectoryAccessConfirmationView::ShowDialog(
        kTestOrigin, base::FilePath(FILE_PATH_LITERAL("/foo/bar/MyProject")),
        base::BindLambdaForTesting([&](PermissionAction result) {
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
  PermissionAction callback_result_ = PermissionAction::IGNORED;
};

IN_PROC_BROWSER_TEST_F(NativeFileSystemDirectoryAccessConfirmationViewTest,
                       AcceptIsntDefaultFocused) {
  ShowUi(std::string());
  EXPECT_NE(widget_->client_view()->AsDialogClientView()->ok_button(),
            widget_->GetFocusManager()->GetFocusedView());
  widget_->Close();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemDirectoryAccessConfirmationViewTest,
                       AcceptRunsCallback) {
  ShowUi(std::string());
  widget_->client_view()->AsDialogClientView()->AcceptWindow();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(PermissionAction::GRANTED, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemDirectoryAccessConfirmationViewTest,
                       CancelRunsCallback) {
  ShowUi(std::string());
  widget_->client_view()->AsDialogClientView()->CancelWindow();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(PermissionAction::DISMISSED, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemDirectoryAccessConfirmationViewTest,
                       CancelsWhenClosed) {
  ShowUi(std::string());
  widget_->Close();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(PermissionAction::DISMISSED, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemDirectoryAccessConfirmationViewTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
