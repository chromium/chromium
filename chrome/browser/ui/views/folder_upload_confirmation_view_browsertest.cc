// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/folder_upload_confirmation_view.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
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

  FolderUploadConfirmationViewTest(const FolderUploadConfirmationViewTest&) =
      delete;
  FolderUploadConfirmationViewTest& operator=(
      const FolderUploadConfirmationViewTest&) = delete;

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

  raw_ptr<views::Widget, AcrossTasksDanglingUntriaged> widget_ = nullptr;

  bool callback_called_ = false;
  std::vector<ui::SelectedFileInfo> callback_files_;
};

IN_PROC_BROWSER_TEST_F(FolderUploadConfirmationViewTest,
                       InitiallyFocusesCancel) {
  ShowUi(std::string());
  // Use GetStoredFocusView() instead of GetFocusedView() because the containing
  // window may not be focused in the test (otherwise this will need to be an
  // interactive_ui_test). For this test, it's enough to know that the cancel
  // button is the one that will get focus when it comes.
  EXPECT_EQ(widget_->widget_delegate()->AsDialogDelegate()->GetCancelButton(),
            widget_->GetFocusManager()->GetStoredFocusView());
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
