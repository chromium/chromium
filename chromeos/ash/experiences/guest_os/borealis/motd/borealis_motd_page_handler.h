// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_PAGE_HANDLER_H_
#define CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_PAGE_HANDLER_H_

#include "base/functional/callback.h"
#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd.mojom.h"
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
  BorealisMOTDPageHandler(
      mojo::PendingReceiver<ash::borealis_motd::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<ash::borealis_motd::mojom::Page> pending_page,
      base::OnceClosure on_page_closed_cb);
  ~BorealisMOTDPageHandler() override;

  // ash::borealis_motd::mojom::PageHandler overrides:
  void OnDismiss() override;

 private:
  mojo::Receiver<ash::borealis_motd::mojom::PageHandler> receiver_;
  mojo::Remote<ash::borealis_motd::mojom::Page> page_;
  base::OnceClosure on_page_closed_cb_;
};

}  // namespace borealis

#endif  // CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_PAGE_HANDLER_H_
