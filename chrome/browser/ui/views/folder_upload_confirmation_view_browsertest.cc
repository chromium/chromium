// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/folder_upload_confirmation_view.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/views/controls/button/label_button.h"

constexpr size_t kTestFileCount = 3;
static const base::FilePath::StringPieceType kTestFileNames[kTestFileCount] = {
    FILE_PATH_LITERAL("a.txt"), FILE_PATH_LITERAL("b.txt"),
    FILE_PATH_LITERAL("c.txt")};

class FolderUploadConfirmationViewTest : public DialogBrowserTest {
 public:
  FolderUploadConfirmationViewTest() {
    for (size_t i = 0; i < kTestFileCount; ++i) {
      ui::SelectedFileInfo file_info;
      file_info.file_path = base::FilePath(kTestFileNames[i]);
      test_files_.push_back(file_info);
    }
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    widget_ = FolderUploadConfirmationView::ShowDialog(
        base::FilePath(FILE_PATH_LITERAL("Desktop")),
        base::BindOnce(&FolderUploadConfirmationViewTest::Callback,
                       base::Unretained(this)),
        test_files_, browser()->tab_strip_model()->GetActiveWebContents());
    content::RunAllPendingInMessageLoop();
  }

  void Callback(const std::vector<ui::SelectedFileInfo>& files) {
    EXPECT_FALSE(callback_called_);
    callback_called_ = true;
    callback_files_ = files;
  }

 protected:
  std::vector<ui::SelectedFileInfo> test_files_;

  views::Widget* widget_ = nullptr;

  bool callback_called_ = false;
  std::vector<ui::SelectedFileInfo> callback_files_;

  DISALLOW_COPY_AND_ASSIGN(FolderUploadConfirmationViewTest);
};

IN_PROC_BROWSER_TEST_F(FolderUploadConfirmationViewTest,
                       InitiallyFocusesCancel) {
  ShowUi(std::string());
  EXPECT_EQ(widget_->widget_delegate()->AsDialogDelegate()->GetCancelButton(),
            widget_->GetFocusManager()->GetFocusedView());
  widget_->Close();
  content::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(FolderUploadConfirmationViewTest,
                       AcceptRunsCallbackWithFileInfo) {
  ShowUi(std::string());
  widget_->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  EXPECT_TRUE(callback_called_);
  ASSERT_EQ(kTestFileCount, callback_files_.size());
  for (size_t i = 0; i < kTestFileCount; ++i)
    EXPECT_EQ(test_files_[i].file_path, callback_files_[i].file_path);
  content::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(FolderUploadConfirmationViewTest,
                       CancelRunsCallbackWithEmptyFileInfo) {
  ShowUi(std::string());
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  EXPECT_TRUE(callback_called_);
  EXPECT_TRUE(callback_files_.empty());
  content::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(FolderUploadConfirmationViewTest, CancelsWhenClosed) {
  ShowUi(std::string());
  widget_->Close();
  EXPECT_TRUE(callback_called_);
  EXPECT_TRUE(callback_files_.empty());
  content::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(FolderUploadConfirmationViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
