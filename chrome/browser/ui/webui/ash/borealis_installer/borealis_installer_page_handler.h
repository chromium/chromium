// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_PAGE_HANDLER_H_

#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class BorealisInstallerPageHandler
    : public ash::borealis_installer::mojom::PageHandler {
 public:
  BorealisInstallerPageHandler(
      mojo::PendingReceiver<ash::borealis_installer::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<ash::borealis_installer::mojom::Page> pending_page);
  ~BorealisInstallerPageHandler() override;

 private:
  mojo::Receiver<ash::borealis_installer::mojom::PageHandler> receiver_;
  mojo::Remote<ash::borealis_installer::mojom::Page> page_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_PAGE_HANDLER_H_
