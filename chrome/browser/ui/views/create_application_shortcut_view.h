// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace extensions {
class Extension;
}

namespace views {
class Checkbox;
}

// A dialog allowing the user to create a desktop shortcut pointing to Chrome
// app.
class CreateChromeApplicationShortcutView : public views::DialogDelegateView,
                                            public views::ButtonListener {
 public:
  CreateChromeApplicationShortcutView(
      Profile* profile,
      const extensions::Extension* app,
      const base::Callback<void(bool)>& close_callback);
  ~CreateChromeApplicationShortcutView() override;

  // Initialize the controls on the dialog.
  void InitControls();

  // DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool Accept() override;
  bool Cancel() override;

  // ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  // Creates a new check-box with the given text and checked state.
  std::unique_ptr<views::Checkbox> AddCheckbox(const base::string16& text,
                                               bool checked);

  // Called when the app's ShortcutInfo (with icon) is loaded.
  void OnAppInfoLoaded(std::unique_ptr<web_app::ShortcutInfo> shortcut_info);

  // Profile in which the shortcuts will be created.
  Profile* profile_;

  base::Callback<void(bool)> close_callback_;

  // May be null if the platform doesn't support a particular location.
  views::Checkbox* desktop_check_box_ = nullptr;
  views::Checkbox* menu_check_box_ = nullptr;
  views::Checkbox* quick_launch_check_box_ = nullptr;

  // Target shortcut and file handler info.
  std::unique_ptr<web_app::ShortcutInfo> shortcut_info_;

  base::WeakPtrFactory<CreateChromeApplicationShortcutView> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(CreateChromeApplicationShortcutView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
