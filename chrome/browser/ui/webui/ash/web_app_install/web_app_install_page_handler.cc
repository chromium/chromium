// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/web_app_install/web_app_install_page_handler.h"

namespace ash::web_app_install {

WebAppInstallPageHandler::WebAppInstallPageHandler(
    mojo::PendingReceiver<ash::web_app_install::mojom::PageHandler>
        pending_page_handler,
    CloseDialogCallback close_dialog_callback)
    : receiver_{this, std::move(pending_page_handler)},
      close_dialog_callback_{std::move(close_dialog_callback)} {}

WebAppInstallPageHandler::~WebAppInstallPageHandler() = default;

void WebAppInstallPageHandler::CloseDialog() {
  // The callback could be null if the close button is clicked a second time
  // before the dialog closes.
  if (close_dialog_callback_) {
    std::move(close_dialog_callback_).Run();
  }
}

}  // namespace ash::web_app_install
