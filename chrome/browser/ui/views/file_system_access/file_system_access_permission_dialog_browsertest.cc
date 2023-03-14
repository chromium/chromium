// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_permission_dialog.h"

#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

using AccessType = FileSystemAccessPermissionRequestManager::Access;
using RequestData = FileSystemAccessPermissionRequestManager::RequestData;
using HandleType = content::FileSystemAccessPermissionContext::HandleType;

class FileSystemAccessPermissionDialogTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    RequestData request(kTestOrigin, base::FilePath(), HandleType::kFile,
                        AccessType::kWrite);
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
    } else {
      CHECK_EQ(name, "default");
      request.path = base::FilePath(FILE_PATH_LITERAL("/foo/README.txt"));
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

  absl::optional<permissions::PermissionAction> result_ = absl::nullopt;
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
