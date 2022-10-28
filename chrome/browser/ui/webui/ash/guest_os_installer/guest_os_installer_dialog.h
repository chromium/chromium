// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_GUEST_OS_INSTALLER_GUEST_OS_INSTALLER_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_GUEST_OS_INSTALLER_GUEST_OS_INSTALLER_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"

namespace ash {

class GuestOSInstallerDialog : public SystemWebDialogDelegate {
 public:
  static void Show(const GURL& page_url);

 private:
  explicit GuestOSInstallerDialog(const GURL& page_url);
  ~GuestOSInstallerDialog() override;

  // SystemWebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;
  std::u16string GetDialogTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldCloseDialogOnEscape() const override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_GUEST_OS_INSTALLER_GUEST_OS_INSTALLER_DIALOG_H_
