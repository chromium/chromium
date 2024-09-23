// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_PAGE_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_types.mojom-forward.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class BorealisInstallerPageHandler
    : public ash::borealis_installer::mojom::PageHandler,
      public borealis::BorealisInstaller::Observer {
 public:
  BorealisInstallerPageHandler(
      mojo::PendingReceiver<ash::borealis_installer::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<ash::borealis_installer::mojom::Page> pending_page,
      base::OnceClosure on_page_closed,
      content::WebUI* web_ui);
  ~BorealisInstallerPageHandler() override;

  void Install() override;
  void ShutDown() override;
  void Launch() override;
  void CancelInstall() override;
  void OnPageClosed() override;
  void OpenStoragePage() override;

  // borealis::BorealisInstaller::Observer implementation.
  void OnStateUpdated(
      borealis::BorealisInstaller::InstallingState new_state) override {}
  void OnProgressUpdated(double fraction_complete) override;
  void OnInstallationEnded(borealis::mojom::InstallResult result,
                           const std::string& error_description) override;
  void OnCancelInitiated() override {}

  // Send a close request to the web page.
  void RequestClosePage();

 private:
  mojo::Receiver<ash::borealis_installer::mojom::PageHandler> receiver_;
  mojo::Remote<ash::borealis_installer::mojom::Page> page_;
  base::OnceClosure on_page_closed_;
  raw_ptr<Profile> profile_;
  base::Time install_start_time_;
  base::ScopedObservation<borealis::BorealisInstaller,
                          borealis::BorealisInstaller::Observer>
      observation_;
  base::WeakPtrFactory<BorealisInstallerPageHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_PAGE_HANDLER_H_
