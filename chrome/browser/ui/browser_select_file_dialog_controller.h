// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_SELECT_FILE_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_SELECT_FILE_DIALOG_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Profile;
class TabStripModel;

namespace content {
class PageNavigator;
}

namespace ui {
class BaseWindow;
}

class BrowserSelectFileDialogController
    : public ui::SelectFileDialog::Listener {
 public:
  BrowserSelectFileDialogController(Profile* profile,
                                    TabStripModel* tab_strip_model,
                                    ui::BaseWindow* base_window,
                                    content::PageNavigator* page_navigator);

  BrowserSelectFileDialogController(const BrowserSelectFileDialogController&) =
      delete;
  BrowserSelectFileDialogController& operator=(
      const BrowserSelectFileDialogController&) = delete;
  ~BrowserSelectFileDialogController() override;

  // Opens a file selection dialog for the browser's currently active tab.
  void OpenFile();

 private:
  // SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file_info, int index) override;
  void FileSelectionCanceled() override;

  // The current file selection dialog. This is a ref-counted object that
  // maintains its own lifetime, but we hold a reference to manage interaction.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  const raw_ref<Profile> profile_;
  const raw_ref<TabStripModel> tab_strip_model_;
  const raw_ref<ui::BaseWindow> base_window_;
  const raw_ref<content::PageNavigator> page_navigator_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_SELECT_FILE_DIALOG_CONTROLLER_H_
