// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_TEST_UTILS_H_

#include <vector>

#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

// A fake ui::SelectFileDialog that selects one or more pre-determined files.
class SelectPredeterminedFileDialog : public ui::SelectFileDialog {
 public:
  SelectPredeterminedFileDialog(std::vector<base::FilePath> result,
                                Listener* listener,
                                std::unique_ptr<ui::SelectFilePolicy> policy);

 protected:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override;

  bool IsRunning(gfx::NativeWindow owning_window) const override;
  void ListenerDestroyed() override;
  bool HasMultipleFileTypeChoicesImpl() override;

 private:
  ~SelectPredeterminedFileDialog() override;
  std::vector<base::FilePath> result_;
};

class SelectPredeterminedFileDialogFactory
    : public ui::SelectFileDialogFactory {
 public:
  explicit SelectPredeterminedFileDialogFactory(
      std::vector<base::FilePath> result);
  ~SelectPredeterminedFileDialogFactory() override;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override;

 private:
  std::vector<base::FilePath> result_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_TEST_UTILS_H_
