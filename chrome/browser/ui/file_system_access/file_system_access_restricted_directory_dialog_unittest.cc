// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/file_system_access/file_system_access_restricted_directory_dialog.h"

#include <optional>

#include "base/test/bind.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/test/test_dialog_model_host.h"
#include "url/origin.h"

using HandleType = content::FileSystemAccessPermissionContext::HandleType;
using SensitiveEntryResult =
    content::FileSystemAccessPermissionContext::SensitiveEntryResult;

using FileSystemAccessRestrictedDirectoryDialogTest = testing::Test;

namespace {

class TestFileSystemAccessRestrictedDirectoryDialog {
 public:
  std::unique_ptr<ui::TestDialogModelHost> CreateDialogModelHost() {
    return std::make_unique<ui::TestDialogModelHost>(
        CreateFileSystemAccessRestrictedDirectoryDialogForTesting(
            kTestOrigin, kTestHandleType,
            base::BindLambdaForTesting(
                [&](SensitiveEntryResult result) { result_ = result; })));
  }

  bool CallbackWasCalled() const { return result_.has_value(); }
  SensitiveEntryResult Result() const {
    CHECK(result_.has_value());
    return result_.value();
  }

 private:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const HandleType kTestHandleType = HandleType::kDirectory;

  std::optional<SensitiveEntryResult> result_ = std::nullopt;
};

TEST_F(FileSystemAccessRestrictedDirectoryDialogTest, Accept) {
  TestFileSystemAccessRestrictedDirectoryDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  ui::TestDialogModelHost::Accept(std::move(host));

  EXPECT_TRUE(test_dialog.CallbackWasCalled());
  EXPECT_EQ(test_dialog.Result(), SensitiveEntryResult::kTryAgain);
}

TEST_F(FileSystemAccessRestrictedDirectoryDialogTest, Cancel) {
  TestFileSystemAccessRestrictedDirectoryDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  ui::TestDialogModelHost::Cancel(std::move(host));

  EXPECT_TRUE(test_dialog.CallbackWasCalled());
  EXPECT_EQ(test_dialog.Result(), SensitiveEntryResult::kAbort);
}

TEST_F(FileSystemAccessRestrictedDirectoryDialogTest, Close) {
  TestFileSystemAccessRestrictedDirectoryDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  ui::TestDialogModelHost::Close(std::move(host));

  EXPECT_TRUE(test_dialog.CallbackWasCalled());
  EXPECT_EQ(test_dialog.Result(), SensitiveEntryResult::kAbort);
}

TEST_F(FileSystemAccessRestrictedDirectoryDialogTest, DestroyWithoutAction) {
  TestFileSystemAccessRestrictedDirectoryDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  ui::TestDialogModelHost::DestroyWithoutAction(std::move(host));

  EXPECT_FALSE(test_dialog.CallbackWasCalled());
}

TEST_F(FileSystemAccessRestrictedDirectoryDialogTest,
       CancelButtonInitiallyFocused) {
  TestFileSystemAccessRestrictedDirectoryDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  EXPECT_EQ(host->GetInitiallyFocusedField(),
            host->GetId(ui::TestDialogModelHost::ButtonId::kCancel));
}

}  // namespace
