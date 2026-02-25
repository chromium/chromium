// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd_page_handler.h"

#include <utility>

#include "content/public/browser/web_contents.h"

namespace borealis {

BorealisMOTDPageHandler::BorealisMOTDPageHandler(
    std::unique_ptr<Delegate> delegate,
    mojo::PendingReceiver<ash::borealis_motd::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<ash::borealis_motd::mojom::Page> pending_page,
    OnPageClosedCallback on_page_closed_cb)
    : delegate_{std::move(delegate)},
      receiver_{this, std::move(pending_page_handler)},
      page_{std::move(pending_page)},
      on_page_closed_cb_{std::move(on_page_closed_cb)} {
  CHECK(delegate_);
}

BorealisMOTDPageHandler::~BorealisMOTDPageHandler() = default;

void BorealisMOTDPageHandler::OnDismiss() {
  if (on_page_closed_cb_) {
    std::move(on_page_closed_cb_).Run(UserMotdAction::kDismiss);
  }
}

void BorealisMOTDPageHandler::OnUninstall() {
  delegate_->UninstallBorealis();

  if (on_page_closed_cb_) {
    std::move(on_page_closed_cb_).Run(UserMotdAction::kUninstall);
  }
}

void BorealisMOTDPageHandler::IsBorealisInstalled(
    IsBorealisInstalledCallback callback) {
  bool is_installed = delegate_->IsBorealisInstalled();
  std::move(callback).Run(is_installed);
}

}  // namespace borealis
