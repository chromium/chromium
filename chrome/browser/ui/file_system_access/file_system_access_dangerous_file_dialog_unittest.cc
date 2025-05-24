// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/file_system_access/file_system_access_dangerous_file_dialog.h"

#include <optional>

#include "base/test/bind.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/test/test_dialog_model_host.h"
#include "url/origin.h"

using DangerousFileResult =
    content::FileSystemAccessPermissionContext::SensitiveEntryResult;

using FileSystemAccessDangerousFileDialogTest = testing::Test;

namespace {

class TestFileSystemAccessDangerousFileDialog {
 public:
  std::unique_ptr<ui::TestDialogModelHost> CreateDialogModelHost() {
    return std::make_unique<ui::TestDialogModelHost>(
        CreateFileSystemAccessDangerousFileDialogForTesting(
            kTestOrigin, kTestPathInfo,
            base::BindLambdaForTesting([&](DangerousFileResult result) {
              callback_called_ = true;
              result_ = result;
            })));
  }

  bool CallbackWasCalled() const { return callback_called_; }
  DangerousFileResult Result() const {
    CHECK(result_.has_value());
    return result_.value();
  }

 private:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const content::PathInfo kTestPathInfo =
      content::PathInfo(FILE_PATH_LITERAL("bar.swf"));

  bool callback_called_ = false;
  std::optional<DangerousFileResult> result_ = std::nullopt;
};

TEST_F(FileSystemAccessDangerousFileDialogTest, Accept) {
  TestFileSystemAccessDangerousFileDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  ui::TestDialogModelHost::Accept(std::move(host));

  EXPECT_TRUE(test_dialog.CallbackWasCalled());
  EXPECT_EQ(test_dialog.Result(), DangerousFileResult::kAllowed);
}

TEST_F(FileSystemAccessDangerousFileDialogTest, Cancel) {
  TestFileSystemAccessDangerousFileDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  ui::TestDialogModelHost::Cancel(std::move(host));

  EXPECT_TRUE(test_dialog.CallbackWasCalled());
  EXPECT_EQ(test_dialog.Result(), DangerousFileResult::kAbort);
}

TEST_F(FileSystemAccessDangerousFileDialogTest, Close) {
  TestFileSystemAccessDangerousFileDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  ui::TestDialogModelHost::Close(std::move(host));

  EXPECT_TRUE(test_dialog.CallbackWasCalled());
  EXPECT_EQ(test_dialog.Result(), DangerousFileResult::kAbort);
}

TEST_F(FileSystemAccessDangerousFileDialogTest, DestroyWithoutAction) {
  TestFileSystemAccessDangerousFileDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  ui::TestDialogModelHost::DestroyWithoutAction(std::move(host));

  EXPECT_FALSE(test_dialog.CallbackWasCalled());
}

TEST_F(FileSystemAccessDangerousFileDialogTest, CancelButtonInitiallyFocused) {
  TestFileSystemAccessDangerousFileDialog test_dialog;
  auto host = test_dialog.CreateDialogModelHost();

  EXPECT_EQ(host->GetInitiallyFocusedField(),
            host->GetId(ui::TestDialogModelHost::ButtonId::kCancel));
}

}  // namespace
