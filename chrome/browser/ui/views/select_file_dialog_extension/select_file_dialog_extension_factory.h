// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_SELECT_FILE_DIALOG_EXTENSION_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_SELECT_FILE_DIALOG_EXTENSION_FACTORY_H_

#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"

class SelectFileDialogExtensionFactory : public ui::SelectFileDialogFactory {
 public:
  SelectFileDialogExtensionFactory();

  SelectFileDialogExtensionFactory(const SelectFileDialogExtensionFactory&) =
      delete;
  SelectFileDialogExtensionFactory& operator=(
      const SelectFileDialogExtensionFactory&) = delete;

  ~SelectFileDialogExtensionFactory() override;

 private:
  // Override from ui::SelectFileDialogFactory:
  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_SELECT_FILE_DIALOG_EXTENSION_FACTORY_H_
