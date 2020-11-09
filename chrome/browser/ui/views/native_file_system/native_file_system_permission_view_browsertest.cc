// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_permission_view.h"

#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/permissions/permission_util.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/button/label_button.h"

using AccessType = NativeFileSystemPermissionRequestManager::Access;
using HandleType = content::NativeFileSystemPermissionContext::HandleType;

class NativeFileSystemPermissionViewTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    NativeFileSystemPermissionView::Request request(
        kTestOrigin, base::FilePath(), HandleType::kFile, AccessType::kWrite);
    if (name == "LongFileName") {
      request.path = base::FilePath(FILE_PATH_LITERAL(
          "/foo/bar/Some Really Really Really Really Long File Name.txt"));
    } else if (name == "Folder") {
      request.path = base::FilePath(FILE_PATH_LITERAL("/bar/MyProject"));
      request.handle_type = HandleType::kDirectory;
    } else if (name == "LongOrigin") {
      request.path = base::FilePath(FILE_PATH_LITERAL("/foo/README.txt"));
      request.origin =
          url::Origin::Create(GURL("https://"
                                   "longextendedsubdomainnamewithoutdashesinord"
                                   "ertotestwordwrapping.appspot.com"));
    } else if (name == "FileOrigin") {
      request.path = base::FilePath(FILE_PATH_LITERAL("/foo/README.txt"));
      request.origin = url::Origin::Create(GURL("file:///foo/bar/bla"));
    } else if (name == "ExtensionOrigin") {
      request.path = base::FilePath(FILE_PATH_LITERAL("/foo/README.txt"));
      request.origin = url::Origin::Create(GURL(
          "chrome-extension://ehoadneljpdggcbbknedodolkkjodefl/capture.html"));
    } else if (name == "FolderRead") {
      request.path = base::FilePath(FILE_PATH_LITERAL("/bar/MyProject"));
      request.handle_type = HandleType::kDirectory;
      request.access = AccessType::kRead;
    } else if (name == "FolderReadWrite") {
      request.path = base::FilePath(FILE_PATH_LITERAL("/bar/MyProject"));
      request.handle_type = HandleType::kDirectory;
      request.access = AccessType::kReadWrite;
    } else if (name == "FileRead") {
      request.path = base::FilePath(FILE_PATH_LITERAL("/foo/README.txt"));
      request.access = AccessType::kRead;
    } else if (name == "FileReadWrite") {
      request.path = base::FilePath(FILE_PATH_LITERAL("/foo/README.txt"));
      request.access = AccessType::kReadWrite;
    } else if (name == "default") {
      request.path = base::FilePath(FILE_PATH_LITERAL("/foo/README.txt"));
    } else {
      NOTREACHED() << "Unimplemented test: " << name;
    }
    widget_ = NativeFileSystemPermissionView::ShowDialog(
        request,
        base::BindLambdaForTesting([&](permissions::PermissionAction result) {
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
  permissions::PermissionAction callback_result_ =
      permissions::PermissionAction::IGNORED;
};

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest,
                       AcceptIsntDefaultFocused) {
  ShowUi("default");
  EXPECT_NE(widget_->widget_delegate()->AsDialogDelegate()->GetOkButton(),
            widget_->GetFocusManager()->GetFocusedView());
  widget_->Close();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest, AcceptRunsCallback) {
  ShowUi("default");
  widget_->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(permissions::PermissionAction::GRANTED, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest, CancelRunsCallback) {
  ShowUi("default");
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(permissions::PermissionAction::DISMISSED, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest, CancelsWhenClosed) {
  ShowUi("default");
  widget_->Close();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(permissions::PermissionAction::DISMISSED, callback_result_);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest,
                       InvokeUi_LongFileName) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest, InvokeUi_Folder) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest,
                       InvokeUi_LongOrigin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest,
                       InvokeUi_FileOrigin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest,
                       InvokeUi_ExtensionOrigin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest,
                       InvokeUi_FolderRead) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest,
                       InvokeUi_FolderReadWrite) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest, InvokeUi_FileRead) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemPermissionViewTest,
                       InvokeUi_FileReadWrite) {
  ShowAndVerifyUi();
}
