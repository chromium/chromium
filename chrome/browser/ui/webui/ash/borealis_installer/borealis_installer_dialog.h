// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_DIALOG_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"

class Profile;

namespace ash {

class BorealisInstallerUI;

// Displays contents of chrome://borealis-installer in a dialog.
class BorealisInstallerDialog : public SystemWebDialogDelegate {
 public:
  using OnLoadedCallback =
      base::OnceCallback<void(base::WeakPtr<BorealisInstallerUI>)>;

  // |on_loaded_callback| is ignored if the dialog is already showing.
  static void Show(Profile* profile,
                   OnLoadedCallback on_loaded_callback = OnLoadedCallback());

 private:
  BorealisInstallerDialog(Profile* profile,
                          OnLoadedCallback on_loaded_callback);
  ~BorealisInstallerDialog() override;

  static void ShowBorealisInstallerDialogIfAllowed(
      Profile* profile,
      OnLoadedCallback on_loaded_callback,
      borealis::BorealisFeatures::AllowStatus status);

  // SystemWebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldCloseDialogOnEscape() const override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  bool OnDialogCloseRequested() override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnWebContentsFinishedLoad() override;

  raw_ptr<Profile> profile_;
  base::WeakPtr<BorealisInstallerUI> installer_ui_ = nullptr;
  OnLoadedCallback on_loaded_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_DIALOG_H_
