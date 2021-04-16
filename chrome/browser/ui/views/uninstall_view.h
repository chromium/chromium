// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class Combobox;
class Label;
}

// UninstallView implements the dialog that confirms Chrome uninstallation
// and asks whether to delete Chrome profile. Also if currently Chrome is set
// as default browser, it asks users whether to set another browser as default.
class UninstallView : public views::DialogDelegateView,
                      public ui::ComboboxModel {
 public:
  METADATA_HEADER(UninstallView);
  explicit UninstallView(int* user_selection,
                         const base::RepeatingClosure& quit_closure);
  UninstallView(const UninstallView&) = delete;
  UninstallView& operator=(const UninstallView&) = delete;
  ~UninstallView() override;

  // Overridden from ui::ComboboxModel:
  int GetItemCount() const override;
  std::u16string GetItemAt(int index) const override;

 private:
  typedef std::map<std::wstring, std::wstring> BrowsersMap;

  // Initializes the controls on the dialog.
  void SetupControls();

  void OnDialogAccepted();
  void OnDialogCancelled();

  views::Label* confirm_label_;
  views::Checkbox* delete_profile_;
  views::Checkbox* change_default_browser_;
  views::Combobox* browsers_combo_;
  std::unique_ptr<BrowsersMap> browsers_;
  int& user_selection_;
  base::RepeatingClosure quit_closure_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_
