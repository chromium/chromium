// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_PAGE_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::app_install {

// Handles communication from the chrome://app-install renderer process to
// the browser process exposing various methods for the JS to invoke.
class AppInstallPageHandler : public mojom::PageHandler {
 public:
  using CloseDialogCallback = base::OnceCallback<void()>;
  explicit AppInstallPageHandler(
      mojom::DialogArgsPtr args,
      base::OnceCallback<void(bool accepted)> dialog_accepted_callback,
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
      CloseDialogCallback close_dialog_callback);

  AppInstallPageHandler(const AppInstallPageHandler&) = delete;
  AppInstallPageHandler& operator=(const AppInstallPageHandler&) = delete;

  ~AppInstallPageHandler() override;

  void OnInstallComplete(bool success);

  // mojom::PageHandler:
  void GetDialogArgs(GetDialogArgsCallback callback) override;
  void CloseDialog() override;
  void InstallApp(InstallAppCallback callback) override;

 private:
  mojom::DialogArgsPtr dialog_args_;
  base::OnceCallback<void(bool accepted)> dialog_accepted_callback_;
  mojo::Receiver<mojom::PageHandler> receiver_;
  CloseDialogCallback close_dialog_callback_;
  InstallAppCallback install_app_callback_;

  base::WeakPtrFactory<AppInstallPageHandler> weak_ptr_factory_{this};
};

}  // namespace ash::app_install

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_PAGE_HANDLER_H_
