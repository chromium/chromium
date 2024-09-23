// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/window/dialog_delegate.h"

class CreateAppShortcutDialogTest;
class PrefService;
class Profile;

namespace extensions {
class Extension;
}

namespace views {
class Checkbox;
}

// A dialog allowing the user to create a desktop shortcut pointing to Chrome
// app.
class CreateChromeApplicationShortcutView : public views::DialogDelegateView {
  METADATA_HEADER(CreateChromeApplicationShortcutView,
                  views::DialogDelegateView)

 public:
  CreateChromeApplicationShortcutView(
      Profile* profile,
      const extensions::Extension* app,
      base::OnceCallback<void(bool)> close_callback);
  CreateChromeApplicationShortcutView(
      Profile* profile,
      const std::string& web_app_id,
      base::OnceCallback<void(bool)> close_callback);
  CreateChromeApplicationShortcutView(
      const CreateChromeApplicationShortcutView&) = delete;
  CreateChromeApplicationShortcutView& operator=(
      const CreateChromeApplicationShortcutView&) = delete;
  ~CreateChromeApplicationShortcutView() override;

  // Initialize the controls on the dialog.
  void InitControls();

  // DialogDelegateView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  std::u16string GetWindowTitle() const override;

 private:
  friend class CreateAppShortcutDialogTest;

  CreateChromeApplicationShortcutView(Profile* profile,
                                      bool is_extension,
                                      base::OnceCallback<void(bool)> cb);

  // Creates a new check-box with the given text and checked state.
  std::unique_ptr<views::Checkbox> AddCheckbox(const std::u16string& text,
                                               const std::string& pref_path);

  void CheckboxPressed(std::string pref_path, views::Checkbox* checkbox);

  // Called when the app's ShortcutInfo (with icon) is loaded.
  void OnAppInfoLoaded(std::unique_ptr<web_app::ShortcutInfo> shortcut_info);

  void OnDialogAccepted();

  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> prefs_;
  bool is_extension_;

  base::OnceCallback<void(bool)> close_callback_;

  // May be null if the platform doesn't support a particular location.
  raw_ptr<views::Checkbox> desktop_check_box_ = nullptr;
  raw_ptr<views::Checkbox> menu_check_box_ = nullptr;
  raw_ptr<views::Checkbox> quick_launch_check_box_ = nullptr;

  // Target shortcut and file handler info.
  std::unique_ptr<web_app::ShortcutInfo> shortcut_info_;

  base::WeakPtrFactory<CreateChromeApplicationShortcutView> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
