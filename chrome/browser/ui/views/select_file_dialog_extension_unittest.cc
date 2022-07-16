// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

const SelectFileDialogExtension::RoutingID kDefaultRoutingID =
    SelectFileDialogExtension::RoutingID();

}  // namespace

// Enumerates possible version of tests. We support extension mode (Chrome App)
// and System App (SWA) mode.
enum TestMode {
  EXTENSION_FILES_APP_MODE,
  SYSTEM_FILES_APP_MODE,
};

// Must be a class so it can be a friend of SelectFileDialogExtension.
class SelectFileDialogExtensionTest
    : public ::testing::TestWithParam<TestMode> {
 public:
  SelectFileDialogExtensionTest() {
    if (GetParam() == SYSTEM_FILES_APP_MODE) {
      feature_list_.InitAndEnableFeature(ash::features::kFilesSWA);
    }
  }

  SelectFileDialogExtensionTest(const SelectFileDialogExtensionTest&) = delete;
  SelectFileDialogExtensionTest& operator=(
      const SelectFileDialogExtensionTest&) = delete;

  static SelectFileDialogExtension* CreateDialog(
      ui::SelectFileDialog::Listener* listener) {
    SelectFileDialogExtension* dialog = new SelectFileDialogExtension(listener,
                                                                      NULL);
    // Simulate the dialog opening.
    EXPECT_FALSE(SelectFileDialogExtension::PendingExists(kDefaultRoutingID));
    dialog->AddPending(kDefaultRoutingID);
    EXPECT_TRUE(SelectFileDialogExtension::PendingExists(kDefaultRoutingID));
    return dialog;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test listener for a SelectFileDialog.
class TestListener : public ui::SelectFileDialog::Listener {
 public:
  TestListener() : selected_(false), file_index_(-1) {}

  TestListener(const TestListener&) = delete;
  TestListener& operator=(const TestListener&) = delete;

  ~TestListener() override {}

  bool selected() const { return selected_; }
  int file_index() const { return file_index_; }

  // ui::SelectFileDialog::Listener implementation
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override {
    selected_ = true;
    file_index_ = index;
  }

 private:
  bool selected_;
  int file_index_;
};

// Client of a SelectFileDialog that deletes itself whenever the dialog
// is closed. This is a common pattern in UI code.
class SelfDeletingClient : public ui::SelectFileDialog::Listener {
 public:
  SelfDeletingClient() {
    dialog_ = SelectFileDialogExtensionTest::CreateDialog(this);
  }

  ~SelfDeletingClient() override {
    if (dialog_.get())
      dialog_->ListenerDestroyed();
  }

  SelectFileDialogExtension* dialog() const { return dialog_.get(); }

  // ui::SelectFileDialog::Listener implementation
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override {
    delete this;
  }

 private:
  scoped_refptr<SelectFileDialogExtension> dialog_;
};

TEST_P(SelectFileDialogExtensionTest, FileSelected) {
  const int kFileIndex = 5;
  auto listener = std::make_unique<TestListener>();
  scoped_refptr<SelectFileDialogExtension> dialog =
      CreateDialog(listener.get());
  // Simulate selecting a file.
  ui::SelectedFileInfo info;
  SelectFileDialogExtension::OnFileSelected(kDefaultRoutingID, info,
                                            kFileIndex);
  // Simulate closing the dialog so the listener gets invoked.
  dialog->ExtensionDialogClosing(nullptr);
  EXPECT_TRUE(listener->selected());
  EXPECT_EQ(kFileIndex, listener->file_index());
}

TEST_P(SelectFileDialogExtensionTest, FileSelectionCanceled) {
  auto listener = std::make_unique<TestListener>();
  scoped_refptr<SelectFileDialogExtension> dialog =
      CreateDialog(listener.get());
  // Simulate cancelling the dialog.
  SelectFileDialogExtension::OnFileSelectionCanceled(kDefaultRoutingID);
  // Simulate closing the dialog so the listener gets invoked.
  dialog->ExtensionDialogClosing(nullptr);
  EXPECT_FALSE(listener->selected());
  EXPECT_EQ(-1, listener->file_index());
}

TEST_P(SelectFileDialogExtensionTest, SelfDeleting) {
  SelfDeletingClient* client = new SelfDeletingClient();
  // Ensure we don't crash or trip an Address Sanitizer warning about
  // use-after-free.
  ui::SelectedFileInfo file_info;
  SelectFileDialogExtension::OnFileSelected(kDefaultRoutingID, file_info, 0);
  // Simulate closing the dialog so the listener gets invoked.
  client->dialog()->ExtensionDialogClosing(nullptr);
}

INSTANTIATE_TEST_SUITE_P(SelectFileDialogExtension,
                         SelectFileDialogExtensionTest,
                         ::testing::Values(EXTENSION_FILES_APP_MODE,
                                           SYSTEM_FILES_APP_MODE));
