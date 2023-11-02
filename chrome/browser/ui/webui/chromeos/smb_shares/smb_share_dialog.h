// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_SHARE_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_SHARE_DIALOG_H_

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {
namespace smb_dialog {

class SmbShareDialog : public SystemWebDialogDelegate {
 public:
  SmbShareDialog(const SmbShareDialog&) = delete;
  SmbShareDialog& operator=(const SmbShareDialog&) = delete;

  // Shows the dialog.
  static void Show();

 protected:
  SmbShareDialog();
  ~SmbShareDialog() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  bool ShouldShowCloseButton() const override;
};

class SmbShareDialogUI : public ui::WebDialogUI {
 public:
  explicit SmbShareDialogUI(content::WebUI* web_ui);

  SmbShareDialogUI(const SmbShareDialogUI&) = delete;
  SmbShareDialogUI& operator=(const SmbShareDialogUI&) = delete;

  ~SmbShareDialogUI() override;
};

}  // namespace smb_dialog
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace smb_dialog {
using ::chromeos::smb_dialog::SmbShareDialog;
}  // namespace smb_dialog
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_SHARE_DIALOG_H_
