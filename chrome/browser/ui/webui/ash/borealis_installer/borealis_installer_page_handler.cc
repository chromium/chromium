// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer_page_handler.h"

namespace ash {

BorealisInstallerPageHandler::BorealisInstallerPageHandler(
    mojo::PendingReceiver<ash::borealis_installer::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<ash::borealis_installer::mojom::Page> pending_page)
    : receiver_{this, std::move(pending_page_handler)},
      page_{std::move(pending_page)} {}

BorealisInstallerPageHandler::~BorealisInstallerPageHandler() = default;

}  // namespace ash
