// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CROSTINI_INSTALLER_CROSTINI_INSTALLER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CROSTINI_INSTALLER_CROSTINI_INSTALLER_PAGE_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/crostini_installer/crostini_installer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crostini {
class CrostiniInstallerUIDelegate;
}  // namespace crostini

namespace ash {

class CrostiniInstallerPageHandler
    : public crostini_installer::mojom::PageHandler {
 public:
  CrostiniInstallerPageHandler(
      crostini::CrostiniInstallerUIDelegate* installer_ui_delegate,
      mojo::PendingReceiver<crostini_installer::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<crostini_installer::mojom::Page> pending_page,
      base::OnceClosure on_page_closed);

  CrostiniInstallerPageHandler(const CrostiniInstallerPageHandler&) = delete;
  CrostiniInstallerPageHandler& operator=(const CrostiniInstallerPageHandler&) =
      delete;

  ~CrostiniInstallerPageHandler() override;

  // crostini_installer::mojom::PageHandler:
  void Install(int64_t disk_size_bytes, const std::string& username) override;
  void Cancel() override;
  void CancelBeforeStart() override;
  void OnPageClosed() override;
  void RequestAmountOfFreeDiskSpace(
      RequestAmountOfFreeDiskSpaceCallback callback) override;

  // Send a close request to the web page.
  void RequestClosePage();

 private:
  void OnProgressUpdate(crostini::mojom::InstallerState installer_state,
                        double progress_fraction);
  void OnInstallFinished(crostini::mojom::InstallerError error);
  void OnCanceled();

  raw_ptr<crostini::CrostiniInstallerUIDelegate> installer_ui_delegate_;
  mojo::Receiver<crostini_installer::mojom::PageHandler> receiver_;
  mojo::Remote<crostini_installer::mojom::Page> page_;
  base::OnceClosure on_page_closed_;

  base::WeakPtrFactory<CrostiniInstallerPageHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CROSTINI_INSTALLER_CROSTINI_INSTALLER_PAGE_HANDLER_H_
