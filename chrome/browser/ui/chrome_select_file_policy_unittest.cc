// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_select_file_policy.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if defined(USE_AURA)
// http://crbug.com/105200
#define MAYBE_ExpectAsynchronousListenerCall \
  DISABLED_ExpectAsynchronousListenerCall
#else
#define MAYBE_ExpectAsynchronousListenerCall ExpectAsynchronousListenerCall
#endif

namespace {

class FileSelectionUser : public ui::SelectFileDialog::Listener {
 public:
  FileSelectionUser() : file_selection_initialisation_in_progress(false) {}

  ~FileSelectionUser() override {
    if (select_file_dialog_.get())
      select_file_dialog_->ListenerDestroyed();
  }

  void StartFileSelection() {
    CHECK(!select_file_dialog_.get());
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(nullptr));

    const base::FilePath file_path;
    const base::string16 title = base::string16();

    file_selection_initialisation_in_progress = true;
    select_file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                    title, file_path, NULL, 0,
                                    base::FilePath::StringType(), NULL, NULL);
    file_selection_initialisation_in_progress = false;
  }

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override {
    ASSERT_FALSE(file_selection_initialisation_in_progress);
  }
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override {
    ASSERT_FALSE(file_selection_initialisation_in_progress);
  }
  void FileSelectionCanceled(void* params) override {
    ASSERT_FALSE(file_selection_initialisation_in_progress);
  }

 private:
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  bool file_selection_initialisation_in_progress;
};

}  // namespace

typedef testing::Test ChromeSelectFilePolicyTest;

// Tests if SelectFileDialog::SelectFile returns asynchronously with
// file-selection dialogs disabled by policy.
TEST_F(ChromeSelectFilePolicyTest, MAYBE_ExpectAsynchronousListenerCall) {
  content::BrowserTaskEnvironment task_environment;

  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());

  std::unique_ptr<FileSelectionUser> file_selection_user(
      new FileSelectionUser());

  // Disallow file-selection dialogs.
  local_state.Get()->SetManagedPref(prefs::kAllowFileSelectionDialogs,
                                    std::make_unique<base::Value>(false));

  file_selection_user->StartFileSelection();
}
