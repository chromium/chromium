// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_INSTALLER_CROSTINI_INSTALLER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_INSTALLER_CROSTINI_INSTALLER_PAGE_HANDLER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_installer_types.mojom-forward.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crostini {
class CrostiniInstallerUIDelegate;
}  // namespace crostini

namespace chromeos {

class CrostiniInstallerPageHandler
    : public chromeos::crostini_installer::mojom::PageHandler {
 public:
  CrostiniInstallerPageHandler(
      crostini::CrostiniInstallerUIDelegate* installer_ui_delegate,
      mojo::PendingReceiver<chromeos::crostini_installer::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<chromeos::crostini_installer::mojom::Page>
          pending_page,
      base::OnceClosure close_dialog_callback);
  ~CrostiniInstallerPageHandler() override;

  // chromeos::crostini_installer::mojom::PageHandler:
  void Install() override;
  void Cancel() override;
  void CancelBeforeStart() override;
  void Close() override;

 private:
  void OnProgressUpdate(crostini::mojom::InstallerState installer_state,
                        double progress_fraction);
  void OnInstallFinished(crostini::mojom::InstallerError error);
  void OnCanceled();

  crostini::CrostiniInstallerUIDelegate* installer_ui_delegate_;
  mojo::Receiver<chromeos::crostini_installer::mojom::PageHandler> receiver_;
  mojo::Remote<chromeos::crostini_installer::mojom::Page> page_;
  base::OnceClosure close_dialog_callback_;

  base::WeakPtrFactory<CrostiniInstallerPageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniInstallerPageHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_INSTALLER_CROSTINI_INSTALLER_PAGE_HANDLER_H_
