// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_PAGE_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/web_app_install/web_app_install.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::web_app_install {

// Handles communication from the chrome://web-app-install renderer process to
// the browser process exposing various methods for the JS to invoke.
class WebAppInstallPageHandler : public mojom::PageHandler {
 public:
  using CloseDialogCallback = base::OnceCallback<void()>;
  explicit WebAppInstallPageHandler(
      mojom::DialogArgsPtr args,
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
      CloseDialogCallback close_dialog_callback);

  WebAppInstallPageHandler(const WebAppInstallPageHandler&) = delete;
  WebAppInstallPageHandler& operator=(const WebAppInstallPageHandler&) = delete;

  ~WebAppInstallPageHandler() override;

  // mojom::PageHandler:
  void GetDialogArgs(GetDialogArgsCallback callback) override;
  void CloseDialog() override;

 private:
  mojom::DialogArgsPtr dialog_args_;
  mojo::Receiver<mojom::PageHandler> receiver_;
  CloseDialogCallback close_dialog_callback_;

  base::WeakPtrFactory<WebAppInstallPageHandler> weak_ptr_factory_{this};
};

}  // namespace ash::web_app_install

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_WEB_APP_INSTALL_WEB_APP_INSTALL_PAGE_HANDLER_H_
