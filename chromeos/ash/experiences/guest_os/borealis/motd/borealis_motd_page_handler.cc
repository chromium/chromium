// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd_page_handler.h"

#include <utility>

namespace borealis {

BorealisMOTDPageHandler::BorealisMOTDPageHandler(
    mojo::PendingReceiver<ash::borealis_motd::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<ash::borealis_motd::mojom::Page> pending_page,
    base::OnceClosure on_page_closed_cb)
    : receiver_{this, std::move(pending_page_handler)},
      page_{std::move(pending_page)},
      on_page_closed_cb_{std::move(on_page_closed_cb)} {}

BorealisMOTDPageHandler::~BorealisMOTDPageHandler() = default;

void BorealisMOTDPageHandler::OnDismiss() {
  if (on_page_closed_cb_) {
    std::move(on_page_closed_cb_).Run();
  }
}

}  // namespace borealis
