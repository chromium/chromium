// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_PAGE_HANDLER_H_
#define CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_PAGE_HANDLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd.mojom.h"
#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace borealis {

// Handles Mojo IPC for the Borealis MOTD WebUI. Receives events from
// BorealisMOTDDialog and invokes the appropriate callbacks. Owned by the
// BorealisMOTDUI WebUI controller; destroyed when the dialog closes.
class BorealisMOTDPageHandler : public ash::borealis_motd::mojom::PageHandler {
 public:
  // BorealisMOTDPageHandler::Delegate is needed to allow the ash MOTD page
  // handler to access the BorealisService needed functions implemented in
  // //chrome
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Checks if borealis is installed; Used to display/hide the
    // "Uninstall" option in the MOTD dialog.
    virtual bool IsBorealisInstalled() = 0;

    // Uninstalls borealis when the user clicks on "Uninstall"
    // in the MOTD dialog
    virtual void UninstallBorealis() = 0;
  };

  using OnPageClosedCallback = base::OnceCallback<void(UserMotdAction)>;
  BorealisMOTDPageHandler(
      std::unique_ptr<Delegate> delegate,
      mojo::PendingReceiver<ash::borealis_motd::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<ash::borealis_motd::mojom::Page> pending_page,
      OnPageClosedCallback on_page_closed_cb);
  ~BorealisMOTDPageHandler() override;

  // ash::borealis_motd::mojom::PageHandler overrides:
  void OnDismiss() override;
  void OnUninstall() override;
  void IsBorealisInstalled(IsBorealisInstalledCallback callback) override;

 private:
  std::unique_ptr<Delegate> delegate_;
  mojo::Receiver<ash::borealis_motd::mojom::PageHandler> receiver_;
  mojo::Remote<ash::borealis_motd::mojom::Page> page_;
  OnPageClosedCallback on_page_closed_cb_;
};

}  // namespace borealis

#endif  // CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_PAGE_HANDLER_H_
