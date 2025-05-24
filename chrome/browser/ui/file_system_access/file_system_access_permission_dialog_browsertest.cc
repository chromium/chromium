// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/file_system_access/file_system_access_permission_dialog.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/test/browser_test.h"
#include "url/origin.h"

using AccessType = FileSystemAccessPermissionRequestManager::Access;
using RequestData = FileSystemAccessPermissionRequestManager::RequestData;
using RequestType = FileSystemAccessPermissionRequestManager::RequestType;
using HandleType = content::FileSystemAccessPermissionContext::HandleType;

class FileSystemAccessPermissionDialogTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    RequestData request(
        RequestType::kNewPermission, kTestOrigin,
        {{content::PathInfo(), HandleType::kFile, AccessType::kWrite}});
    if (name == "LongFileName") {
      request.file_request_data[0].path_info =
          content::PathInfo(FILE_PATH_LITERAL(
              "/foo/bar/Some Really Really Really Really Long File Name.txt"));
    } else if (name == "Folder") {
      request.file_request_data[0].path_info =
          content::PathInfo(FILE_PATH_LITERAL("/bar/MyProject"));
      request.file_request_data[0].handle_type = HandleType::kDirectory;
    } else if (name == "LongOrigin") {
      request.file_request_data[0].path_info =
          content::PathInfo(FILE_PATH_LITERAL("/foo/README.txt"));
      request.origin =
          url::Origin::Create(GURL("https://"
                                   "longextendedsubdomainnamewithoutdashesinord"
                                   "ertotestwordwrapping.appspot.com"));
    } else if (name == "FileOrigin") {
      request.file_request_data[0].path_info =
          content::PathInfo(FILE_PATH_LITERAL("/foo/README.txt"));
      request.origin = url::Origin::Create(GURL("file:///foo/bar/bla"));
    } else if (name == "ExtensionOrigin") {
      request.file_request_data[0].path_info =
          content::PathInfo(FILE_PATH_LITERAL("/foo/README.txt"));
      request.origin = url::Origin::Create(GURL(
          "chrome-extension://ehoadneljpdggcbbknedodolkkjodefl/capture.html"));
    } else if (name == "FolderRead") {
      request.file_request_data[0].path_info =
          content::PathInfo(FILE_PATH_LITERAL("/bar/MyProject"));
      request.file_request_data[0].handle_type = HandleType::kDirectory;
      request.file_request_data[0].access = AccessType::kRead;
    } else if (name == "FolderReadWrite") {
      request.file_request_data[0].path_info =
          content::PathInfo(FILE_PATH_LITERAL("/bar/MyProject"));
      request.file_request_data[0].handle_type = HandleType::kDirectory;
      request.file_request_data[0].access = AccessType::kReadWrite;
    } else if (name == "FileRead") {
      request.file_request_data[0].path_info =
          content::PathInfo(FILE_PATH_LITERAL("/foo/README.txt"));
      request.file_request_data[0].access = AccessType::kRead;
    } else if (name == "FileReadWrite") {
      request.file_request_data[0].path_info =
          content::PathInfo(FILE_PATH_LITERAL("/foo/README.txt"));
      request.file_request_data[0].access = AccessType::kReadWrite;
    } else {
      CHECK_EQ(name, "default");
      request.file_request_data[0].path_info =
          content::PathInfo(FILE_PATH_LITERAL("/foo/README.txt"));
    }
    ShowFileSystemAccessPermissionDialog(
        request,
        base::BindLambdaForTesting(
            [&](permissions::PermissionAction result) { result_ = result; }),
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  bool CallbackWasCalled() const { return result_.has_value(); }
  permissions::PermissionAction Result() const {
    CHECK(result_.has_value());
    return result_.value();
  }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));

  std::optional<permissions::PermissionAction> result_ = std::nullopt;
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessPermissionDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessPermissionDialogTest,
                       InvokeUi_LongFileName) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessPermissionDialogTest, InvokeUi_Folder) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessPermissionDialogTest,
                       InvokeUi_LongOrigin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessPermissionDialogTest,
                       InvokeUi_FileOrigin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessPermissionDialogTest,
                       InvokeUi_ExtensionOrigin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessPermissionDialogTest,
                       InvokeUi_FolderRead) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessPermissionDialogTest,
                       InvokeUi_FolderReadWrite) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessPermissionDialogTest,
                       InvokeUi_FileRead) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessPermissionDialogTest,
                       InvokeUi_FileReadWrite) {
  ShowAndVerifyUi();
}
