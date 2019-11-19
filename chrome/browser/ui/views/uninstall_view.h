// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_

#include <map>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class Combobox;
class Label;
}

// UninstallView implements the dialog that confirms Chrome uninstallation
// and asks whether to delete Chrome profile. Also if currently Chrome is set
// as default browser, it asks users whether to set another browser as default.
class UninstallView : public views::ButtonListener,
                      public views::DialogDelegateView,
                      public ui::ComboboxModel {
 public:
  explicit UninstallView(int* user_selection,
                         const base::Closure& quit_closure);
  ~UninstallView() override;

  // Overridden form views::ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from views::DialogDelegateView:
  bool Accept() override;
  bool Cancel() override;

  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;

  // Overridden from ui::ComboboxModel:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;

 private:
  typedef std::map<base::string16, base::string16> BrowsersMap;

  // Initializes the controls on the dialog.
  void SetupControls();

  views::Label* confirm_label_;
  views::Checkbox* delete_profile_;
  views::Checkbox* change_default_browser_;
  views::Combobox* browsers_combo_;
  std::unique_ptr<BrowsersMap> browsers_;
  int& user_selection_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(UninstallView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_
