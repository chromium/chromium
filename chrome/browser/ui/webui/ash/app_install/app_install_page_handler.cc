// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_page_handler.h"

#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"

namespace ash::app_install {

AppInstallPageHandler::AppInstallPageHandler(
    mojom::DialogArgsPtr args,
    mojo::PendingReceiver<mojom::PageHandler>
        pending_page_handler,
    CloseDialogCallback close_dialog_callback)
    : dialog_args_{std::move(args)},
      receiver_{this, std::move(pending_page_handler)},
      close_dialog_callback_{std::move(close_dialog_callback)} {}

AppInstallPageHandler::~AppInstallPageHandler() = default;

void AppInstallPageHandler::GetDialogArgs(GetDialogArgsCallback callback) {
  std::move(callback).Run(dialog_args_ ? dialog_args_.Clone()
                                       : mojom::DialogArgs::New());
}

void AppInstallPageHandler::CloseDialog() {
  // The callback could be null if the close button is clicked a second time
  // before the dialog closes.
  if (close_dialog_callback_) {
    std::move(close_dialog_callback_).Run();
  }
}

}  // namespace ash::app_install
