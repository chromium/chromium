// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SMB_SHARES_SMB_CREDENTIALS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SMB_SHARES_SMB_CREDENTIALS_DIALOG_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace color_change_listener::mojom {
class PageHandler;
}  // namespace color_change_listener::mojom

namespace ui {
class ColorChangeHandler;
}

namespace ash::smb_dialog {

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

class SmbCredentialsDialogUI;

// WebUIConfig for chrome://smb-credentials-dialog
class SmbCredentialsDialogUIConfig
    : public content::DefaultWebUIConfig<SmbCredentialsDialogUI> {
 public:
  SmbCredentialsDialogUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISmbCredentialsHost) {}
};

class SmbCredentialsDialogUI : public ui::WebDialogUI {
 public:
  explicit SmbCredentialsDialogUI(content::WebUI* web_ui);

  SmbCredentialsDialogUI(const SmbCredentialsDialogUI&) = delete;
  SmbCredentialsDialogUI& operator=(const SmbCredentialsDialogUI&) = delete;

  ~SmbCredentialsDialogUI() override;

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  void OnUpdateCredentials(const std::string& username,
                           const std::string& password);
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::smb_dialog

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SMB_SHARES_SMB_CREDENTIALS_DIALOG_H_
