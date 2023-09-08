// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_permission_dialog.h"

#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/test/test_dialog_model_host.h"
#include "url/origin.h"

using AccessType = FileSystemAccessPermissionRequestManager::Access;
using HandleType = content::FileSystemAccessPermissionContext::HandleType;
using RequestData = FileSystemAccessPermissionRequestManager::RequestData;
using RequestType = FileSystemAccessPermissionRequestManager::RequestType;

using FileSystemAccessPermissionDialogTest = BrowserWithTestWindowTest;

class TestFileSystemAccessPermissionDialog {
 public:
  std::unique_ptr<ui::TestDialogModelHost> CreateDialogModelHost() {
    RequestData request(RequestType::kNewPermission, kTestOrigin,
                        {{kTestPath, HandleType::kFile, AccessType::kRead}});
    return std::make_unique<ui::TestDialogModelHost>(
        CreateFileSystemAccessPermissionDialogForTesting(
            request, base::BindLambdaForTesting(
                         [&](permissions::PermissionAction result) {
                           result_ = result;
                         })));
  }

  bool CallbackWasCalled() const { return result_.has_value(); }
  permissions::PermissionAction Result() const {
    CHECK(result_.has_value());
    return result_.value();
  }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const base::FilePath kTestPath =
      base::FilePath(FILE_PATH_LITERAL("/foo/bar.txt"));

  absl::optional<permissions::PermissionAction> result_ = absl::nullopt;
};

TEST_F(FileSystemAccessPermissionDialogTest, Accept) {
  TestFileSystemAccessPermissionDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  ui::TestDialogModelHost::Accept(std::move(host));

  EXPECT_TRUE(test_dialog.CallbackWasCalled());
  EXPECT_EQ(test_dialog.Result(), permissions::PermissionAction::GRANTED);
}

TEST_F(FileSystemAccessPermissionDialogTest, Cancel) {
  TestFileSystemAccessPermissionDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  ui::TestDialogModelHost::Cancel(std::move(host));

  EXPECT_TRUE(test_dialog.CallbackWasCalled());
  EXPECT_EQ(test_dialog.Result(), permissions::PermissionAction::DISMISSED);
}

TEST_F(FileSystemAccessPermissionDialogTest, Close) {
  TestFileSystemAccessPermissionDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  ui::TestDialogModelHost::Close(std::move(host));

  EXPECT_TRUE(test_dialog.CallbackWasCalled());
  EXPECT_EQ(test_dialog.Result(), permissions::PermissionAction::DISMISSED);
}

TEST_F(FileSystemAccessPermissionDialogTest, DestroyWithoutAction) {
  TestFileSystemAccessPermissionDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  ui::TestDialogModelHost::DestroyWithoutAction(std::move(host));

  EXPECT_FALSE(test_dialog.CallbackWasCalled());
}

TEST_F(FileSystemAccessPermissionDialogTest, CancelButtonInitiallyFocused) {
  TestFileSystemAccessPermissionDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  EXPECT_EQ(host->GetInitiallyFocusedField(),
            host->GetId(ui::TestDialogModelHost::ButtonId::kCancel));
}
