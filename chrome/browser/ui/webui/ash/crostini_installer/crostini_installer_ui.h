// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CROSTINI_INSTALLER_CROSTINI_INSTALLER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CROSTINI_INSTALLER_CROSTINI_INSTALLER_UI_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/crostini_installer/crostini_installer.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

class CrostiniInstallerPageHandler;
class CrostiniInstallerUI;

// WebUIConfig for chrome://crostini-installer
class CrostiniInstallerUIConfig
    : public content::DefaultWebUIConfig<CrostiniInstallerUI> {
 public:
  CrostiniInstallerUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUICrostiniInstallerHost) {}
};

// The WebUI for chrome://crostini-installer
class CrostiniInstallerUI
    : public ui::MojoWebDialogUI,
      public crostini_installer::mojom::PageHandlerFactory {
 public:
  explicit CrostiniInstallerUI(content::WebUI* web_ui);

  CrostiniInstallerUI(const CrostiniInstallerUI&) = delete;
  CrostiniInstallerUI& operator=(const CrostiniInstallerUI&) = delete;

  ~CrostiniInstallerUI() override;

  // Send a close request to the web page. Return true if the page is already
  // closed.
  bool RequestClosePage();

  void ClickInstallForTesting();

  // Instantiates implementor of the mojom::PageHandlerFactory
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<crostini_installer::mojom::PageHandlerFactory>
          pending_receiver);

  base::WeakPtr<CrostiniInstallerUI> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // crostini_installer::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<crostini_installer::mojom::Page> pending_page,
      mojo::PendingReceiver<crostini_installer::mojom::PageHandler>
          pending_page_handler) override;

  void OnPageClosed();

  std::unique_ptr<CrostiniInstallerPageHandler> page_handler_;
  mojo::Receiver<crostini_installer::mojom::PageHandlerFactory>
      page_factory_receiver_{this};
  bool page_closed_ = false;

  base::WeakPtrFactory<CrostiniInstallerUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CROSTINI_INSTALLER_CROSTINI_INSTALLER_UI_H_
