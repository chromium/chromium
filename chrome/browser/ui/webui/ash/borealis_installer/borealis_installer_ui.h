// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_UI_H_

#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer.mojom.h"
#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

class BorealisInstallerUI;

// WebUIConfig for chrome://borealis-installer
class BorealisInstallerUIConfig
    : public content::DefaultWebUIConfig<BorealisInstallerUI> {
 public:
  BorealisInstallerUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIBorealisInstallerHost) {}
};

// The WebUI for chrome://borealis-installer
class BorealisInstallerUI
    : public ui::MojoWebDialogUI,
      public ash::borealis_installer::mojom::PageHandlerFactory {
 public:
  explicit BorealisInstallerUI(content::WebUI* web_ui);
  ~BorealisInstallerUI() override;

  // Instantiates implementor of the mojom::PageHandlerFactory
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<borealis_installer::mojom::PageHandlerFactory>
          pending_receiver);

  // Send a close request to the web page. Return true if the page is already
  // closed.
  bool RequestClosePage();

  base::WeakPtr<BorealisInstallerUI> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void BindPageHandlerFactory(
      mojo::PendingReceiver<ash::borealis_installer::mojom::PageHandlerFactory>
          pending_receiver);

  // ash::borealis_installer::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<ash::borealis_installer::mojom::Page> pending_page,
      mojo::PendingReceiver<ash::borealis_installer::mojom::PageHandler>
          pending_page_handler) override;
  void OnPageClosed();

  std::unique_ptr<BorealisInstallerPageHandler> page_handler_;
  mojo::Receiver<ash::borealis_installer::mojom::PageHandlerFactory>
      page_factory_receiver_{this};
  bool page_closed_;
  raw_ptr<content::WebUI> web_ui_;
  base::WeakPtrFactory<BorealisInstallerUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_UI_H_
