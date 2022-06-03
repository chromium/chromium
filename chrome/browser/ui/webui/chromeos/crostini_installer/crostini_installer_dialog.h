// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_INSTALLER_CROSTINI_INSTALLER_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_INSTALLER_CROSTINI_INSTALLER_DIALOG_H_

#include "base/callback.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

class Profile;

namespace chromeos {

class CrostiniInstallerUI;

class CrostiniInstallerDialog : public SystemWebDialogDelegate {
 public:
  using OnLoadedCallback = base::OnceCallback<void(CrostiniInstallerUI*)>;

  // |on_loaded_callback| is ignored if the dialog is already showing.
  static void Show(Profile* profile,
                   OnLoadedCallback on_loaded_callback = OnLoadedCallback());

 private:
  CrostiniInstallerDialog(Profile* profile,
                          OnLoadedCallback on_loaded_callback);
  ~CrostiniInstallerDialog() override;

  // SystemWebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldCloseDialogOnEscape() const override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  bool OnDialogCloseRequested() override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  void OnWebContentsFinishedLoad() override;

  Profile* profile_;
  CrostiniInstallerUI* installer_ui_ = nullptr;
  OnLoadedCallback on_loaded_callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_INSTALLER_CROSTINI_INSTALLER_DIALOG_H_
