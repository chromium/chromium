// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_UI_H_

#include "chrome/browser/ui/webui/ash/web_app_install//web_app_install_page_handler.h"
#include "content/public/browser/webui_config.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash::web_app_install {

// The WebUI for chrome://web-app-install-dialog, used for confirming web app
// installation.
class WebAppInstallDialogUI : public ui::MojoWebDialogUI,
                              public mojom::PageHandlerFactory {
 public:
  explicit WebAppInstallDialogUI(content::WebUI* web_ui);
  WebAppInstallDialogUI(const WebAppInstallDialogUI&) = delete;
  WebAppInstallDialogUI& operator=(const WebAppInstallDialogUI&) = delete;
  ~WebAppInstallDialogUI() override;

  void SetDialogArgs(mojom::DialogArgsPtr args);

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<mojom::PageHandlerFactory> receiver);

  // mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler) override;

 private:
  void CloseDialog();

  mojom::DialogArgsPtr dialog_args_;
  std::unique_ptr<WebAppInstallPageHandler> page_handler_;
  mojo::Receiver<mojom::PageHandlerFactory> factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// WebUIConfig for chrome://web-app-install-dialog
class WebAppInstallDialogUIConfig
    : public content::DefaultWebUIConfig<WebAppInstallDialogUI> {
 public:
  WebAppInstallDialogUIConfig();

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace ash::web_app_install

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_UI_H_
