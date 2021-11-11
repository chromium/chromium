// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_CREDENTIALS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_CREDENTIALS_DIALOG_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {
namespace smb_dialog {

class SmbCredentialsDialog : public SystemWebDialogDelegate {
 public:
  using RequestCallback = base::OnceCallback<void(bool canceled,
                                                  const std::string& username,
                                                  const std::string& password)>;

  SmbCredentialsDialog(const SmbCredentialsDialog&) = delete;
  SmbCredentialsDialog& operator=(const SmbCredentialsDialog&) = delete;

  // Shows the dialog, and runs |callback| when the user responds with a
  // username/password, or the dialog is closed. If a dialog is currently being
  // shown for |mount_id|, the existing dialog will be focused and its callback
  // will be replaced with |callback|.
  static void Show(const std::string& mount_id,
                   const std::string& share_path,
                   RequestCallback callback);

  // Respond to the dialog being show with a username/password.
  void Respond(const std::string& username, const std::string& password);

 protected:
  SmbCredentialsDialog(const std::string& mount_id,
                       const std::string& share_path,
                       RequestCallback callback);
  ~SmbCredentialsDialog() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  bool ShouldShowCloseButton() const override;

 private:
  const std::string mount_id_;
  const std::string share_path_;
  RequestCallback callback_;
};

class SmbCredentialsDialogUI : public ui::WebDialogUI {
 public:
  explicit SmbCredentialsDialogUI(content::WebUI* web_ui);

  SmbCredentialsDialogUI(const SmbCredentialsDialogUI&) = delete;
  SmbCredentialsDialogUI& operator=(const SmbCredentialsDialogUI&) = delete;

  ~SmbCredentialsDialogUI() override;

 private:
  void OnUpdateCredentials(const std::string& username,
                           const std::string& password);
};

}  // namespace smb_dialog
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace smb_dialog {
using ::chromeos::smb_dialog::SmbCredentialsDialog;
}  // namespace smb_dialog
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_CREDENTIALS_DIALOG_H_
