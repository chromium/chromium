// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_PAGE_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog_args.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::app_install {

// Handles communication from the chrome://app-install renderer process to
// the browser process exposing various methods for the JS to invoke.
class AppInstallPageHandler : public mojom::PageHandler,
                              public mojom::AppInfoActions,
                              public mojom::ConnectionErrorActions {
 public:
  // Whether the app install dialog is enabled and should auto accept
  // installation without actual user input.
  static bool GetAutoAcceptForTesting();
  static void SetAutoAcceptForTesting(bool auto_accept);

  using CloseDialogCallback = base::OnceCallback<void()>;
  explicit AppInstallPageHandler(
      Profile* profile,
      std::optional<AppInstallDialogArgs> dialog_args,
      CloseDialogCallback close_dialog_callback,
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler);

  AppInstallPageHandler(const AppInstallPageHandler&) = delete;
  AppInstallPageHandler& operator=(const AppInstallPageHandler&) = delete;

  ~AppInstallPageHandler() override;

  void SetDialogArgs(AppInstallDialogArgs dialog_args);
  void OnInstallComplete(
      bool success,
      std::optional<base::OnceCallback<void(bool accepted)>> retry_callback);

  // mojom::PageHandler:
  void GetDialogArgs(GetDialogArgsCallback callback) override;
  void CloseDialog() override;

  // mojom::AppInfoActions:
  void InstallApp(InstallAppCallback callback) override;
  void LaunchApp() override;

  // mojom::ConnectionErrorActions:
  void TryAgain() override;

 private:
  mojom::DialogArgsPtr ConvertDialogArgsToMojom(
      const AppInstallDialogArgs& dialog_args);

  raw_ptr<Profile> profile_;
  std::optional<AppInstallDialogArgs> dialog_args_;
  std::vector<GetDialogArgsCallback> pending_dialog_args_callbacks_;
  InstallAppCallback install_app_callback_;
  CloseDialogCallback close_dialog_callback_;
  mojo::Receiver<mojom::PageHandler> page_handler_receiver_;
  mojo::Receiver<mojom::AppInfoActions> app_info_actions_receiver_;
  mojo::Receiver<mojom::ConnectionErrorActions>
      connection_error_actions_receiver_;

  base::WeakPtrFactory<AppInstallPageHandler> weak_ptr_factory_{this};
};

}  // namespace ash::app_install

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_PAGE_HANDLER_H_
