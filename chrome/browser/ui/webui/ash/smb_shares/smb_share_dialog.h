// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SMB_SHARES_SMB_SHARE_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SMB_SHARES_SMB_SHARE_DIALOG_H_

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

class SmbShareDialogUI;

// WebUIConfig for chrome://smb-share-dialog
class SmbShareDialogUIConfig
    : public content::DefaultWebUIConfig<SmbShareDialogUI> {
 public:
  SmbShareDialogUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISmbShareHost) {}
};

class SmbShareDialogUI : public ui::WebDialogUI {
 public:
  explicit SmbShareDialogUI(content::WebUI* web_ui);

  SmbShareDialogUI(const SmbShareDialogUI&) = delete;
  SmbShareDialogUI& operator=(const SmbShareDialogUI&) = delete;

  ~SmbShareDialogUI() override;

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::smb_dialog

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SMB_SHARES_SMB_SHARE_DIALOG_H_
