// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/select_file_dialog_impl_gtk.h"

#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/libgtkui/gtk_ui.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::RunLoop;
using content::BrowserThread;

class SelectFileDialogImplGtkTest : public testing::Test {
 public:
  SelectFileDialogImplGtkTest() = default;

  void SetUp() override {
    gtk_ui_.reset(BuildGtkUi());
    ui::ShellDialogLinux::SetInstance(gtk_ui_.get());
  }

  void TearDown() override {
    ui::ShellDialogLinux::SetInstance(nullptr);
    gtk_ui_.reset();
  }

 private:
  std::unique_ptr<views::LinuxUI> gtk_ui_;
};

namespace libgtkui {

class FilePicker : public ui::SelectFileDialog::Listener {
 public:
  explicit FilePicker(ui::SelectFileDialog::Type type) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(nullptr));

    ui::SelectFileDialog::FileTypeInfo file_types;
    file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;
    const base::FilePath file_path;
    select_file_dialog_->SelectFile(
        type, base::string16(), file_path, &file_types, 0,
        base::FilePath::StringType(), nullptr, nullptr);
  }

  ~FilePicker() override {
    SelectFileDialogImplGTK* file_dialog =
        static_cast<SelectFileDialogImplGTK*>(select_file_dialog_.get());

    while (!file_dialog->dialogs_.empty())
      gtk_widget_destroy(*(file_dialog->dialogs_.begin()));

    select_file_dialog_->ListenerDestroyed();
  }

  bool canCreateFolder() {
    return gtk_file_chooser_get_create_folders(getChooser());
  }

  bool canSelectMultiple() {
    return gtk_file_chooser_get_select_multiple(getChooser());
  }

  const gchar* getTitle() {
    return gtk_window_get_title(
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(getChooser()))));
  }

  // SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override {}

 private:
  // Dialog box used for opening and saving files.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  GtkFileChooser* getChooser() {
    auto* dialog =
        static_cast<SelectFileDialogImplGTK*>(select_file_dialog_.get());
    return GTK_FILE_CHOOSER(*(dialog->dialogs_.begin()));
  }

  DISALLOW_COPY_AND_ASSIGN(FilePicker);
};

// Note: The tests below were disabled for defined(ADDRESS_SANITIZER) with the
// following reasoning:
// Glib runs glib_init() when it is loaded by dl, and in the process
// allocates some memory that is intentionally never freed.
// Targeted suppression of the memory leak was not possible.

// Flaky, see crbug.com/853079.
TEST_F(SelectFileDialogImplGtkTest, DISABLED_SelectExistingFolder) {
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());

  FilePicker file_picker(ui::SelectFileDialog::SELECT_EXISTING_FOLDER);

  EXPECT_FALSE(file_picker.canSelectMultiple());
  EXPECT_FALSE(file_picker.canCreateFolder());
  EXPECT_STREQ("Select Folder", file_picker.getTitle());

  base::ThreadPoolInstance::Get()->FlushForTesting();
  RunLoop().RunUntilIdle();
}

// Flaky, see crbug.com/853079.
TEST_F(SelectFileDialogImplGtkTest, DISABLED_SelectUploadFolder) {
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());

  FilePicker file_picker(ui::SelectFileDialog::SELECT_UPLOAD_FOLDER);

  EXPECT_FALSE(file_picker.canSelectMultiple());
  EXPECT_FALSE(file_picker.canCreateFolder());
  EXPECT_STREQ("Select Folder to Upload", file_picker.getTitle());

  base::ThreadPoolInstance::Get()->FlushForTesting();
  RunLoop().RunUntilIdle();
}

// Flaky, see crbug.com/853079.
TEST_F(SelectFileDialogImplGtkTest, DISABLED_SelectFolder) {
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());

  FilePicker file_picker(ui::SelectFileDialog::SELECT_FOLDER);

  EXPECT_FALSE(file_picker.canSelectMultiple());
  EXPECT_TRUE(file_picker.canCreateFolder());
  EXPECT_STREQ("Select Folder", file_picker.getTitle());

  base::ThreadPoolInstance::Get()->FlushForTesting();
  RunLoop().RunUntilIdle();
}

}  // namespace libgtkui
