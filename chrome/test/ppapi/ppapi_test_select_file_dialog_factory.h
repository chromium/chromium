// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PPAPI_PPAPI_TEST_SELECT_FILE_DIALOG_FACTORY_H_
#define CHROME_TEST_PPAPI_PPAPI_TEST_SELECT_FILE_DIALOG_FACTORY_H_

#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

class PPAPITestSelectFileDialogFactory final
    : public ui::SelectFileDialogFactory {
 public:
  using SelectedFileInfoList = std::vector<ui::SelectedFileInfo>;

  enum Mode {
    RESPOND_WITH_FILE_LIST,
    CANCEL,
    REPLACE_BASENAME,
    NOT_REACHED,
  };

  PPAPITestSelectFileDialogFactory(
      Mode mode,
      const SelectedFileInfoList& selected_file_info);
  // SelectFileDialogFactory
  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override;

 private:
  std::vector<ui::SelectedFileInfo> selected_file_info_;
  Mode mode_;
};

#endif  // CHROME_TEST_PPAPI_PPAPI_TEST_SELECT_FILE_DIALOG_FACTORY_H_
