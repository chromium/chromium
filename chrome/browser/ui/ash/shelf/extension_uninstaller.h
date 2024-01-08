// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_EXTENSION_UNINSTALLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_EXTENSION_UNINSTALLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

// ExtensionUninstaller runs the extension uninstall flow. It shows the
// extension uninstall dialog and wait for user to confirm or cancel the
// uninstall.
class ExtensionUninstaller
    : public extensions::ExtensionUninstallDialog::Delegate {
 public:
  // If |parent_window| is specified, the uninstall dialog will be created as a
  // modal dialog anchored at |parent_window|. Otherwise, the browser
  // window will be used as the anchor.
  ExtensionUninstaller(Profile* profile,
                       const std::string& extension_id,
                       gfx::NativeWindow parent_window = gfx::NativeWindow());

  ExtensionUninstaller(const ExtensionUninstaller&) = delete;
  ExtensionUninstaller& operator=(const ExtensionUninstaller&) = delete;

  ~ExtensionUninstaller() override;

  void Run();

 private:
  // Overridden from ExtensionUninstallDialog::Delegate:
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const std::u16string& error) override;

  void CleanUp();

  raw_ptr<Profile> profile_;
  std::string app_id_;
  gfx::NativeWindow parent_window_ = gfx::NativeWindow();
  std::unique_ptr<extensions::ExtensionUninstallDialog> dialog_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_EXTENSION_UNINSTALLER_H_
