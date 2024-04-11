// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_UI_H_

#include "chrome/browser/ui/webui/ash/app_install/app_install_page_handler.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "content/public/browser/webui_config.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}

namespace ash::app_install {

// The WebUI for chrome://app-install-dialog, used for confirming app
// installation.
class AppInstallDialogUI : public ui::MojoWebDialogUI,
                           public mojom::PageHandlerFactory {
 public:
  explicit AppInstallDialogUI(content::WebUI* web_ui);
  AppInstallDialogUI(const AppInstallDialogUI&) = delete;
  AppInstallDialogUI& operator=(const AppInstallDialogUI&) = delete;
  ~AppInstallDialogUI() override;

  void SetDialogArgs(mojom::DialogArgsPtr args);
  void SetPackageId(apps::PackageId package_id);
  void SetDialogCallback(
      base::OnceCallback<void(bool accepted)> dialog_accepted_callback);
  void SetTryAgainCallback(base::OnceClosure try_again_callback);
  void SetInstallComplete(
      bool success,
      std::optional<base::OnceCallback<void(bool accepted)>> retry_callback);

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<mojom::PageHandlerFactory> receiver);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  // mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler) override;

 private:
  void CloseDialog();

  mojom::DialogArgsPtr dialog_args_;
  apps::PackageId package_id_;
  base::OnceCallback<void(bool accepted)> dialog_accepted_callback_;
  base::OnceClosure try_again_callback_;
  std::unique_ptr<AppInstallPageHandler> page_handler_;
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  mojo::Receiver<mojom::PageHandlerFactory> factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// WebUIConfig for chrome://app-install-dialog
class AppInstallDialogUIConfig
    : public content::DefaultWebUIConfig<AppInstallDialogUI> {
 public:
  AppInstallDialogUIConfig();

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace ash::app_install

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_UI_H_
