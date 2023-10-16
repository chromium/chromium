// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/web_app_install/web_app_install_page_handler.h"

namespace ash::web_app_install {

WebAppInstallPageHandler::WebAppInstallPageHandler(
    mojo::PendingReceiver<ash::web_app_install::mojom::PageHandler>
        pending_page_handler)
    : receiver_{this, std::move(pending_page_handler)} {}

WebAppInstallPageHandler::~WebAppInstallPageHandler() = default;

}  // namespace ash::web_app_install
