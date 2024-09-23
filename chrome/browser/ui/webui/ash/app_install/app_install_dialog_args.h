// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_ARGS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_ARGS_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace ash::app_install {

struct AppInfoArgs {
  AppInfoArgs();
  AppInfoArgs(AppInfoArgs&&);
  AppInfoArgs(const AppInfoArgs&) = delete;
  ~AppInfoArgs();
  AppInfoArgs& operator=(AppInfoArgs&&);
  AppInfoArgs& operator=(const AppInfoArgs&) = delete;

  apps::PackageId package_id;
  mojom::AppInfoDataPtr data;
  base::OnceCallback<void(bool accepted)> dialog_accepted_callback;
};

struct NoAppErrorArgs {};

struct ConnectionErrorArgs {
  ConnectionErrorArgs();
  ConnectionErrorArgs(ConnectionErrorArgs&&);
  ConnectionErrorArgs(const ConnectionErrorArgs&) = delete;
  ~ConnectionErrorArgs();
  ConnectionErrorArgs& operator=(ConnectionErrorArgs&&);
  ConnectionErrorArgs& operator=(const ConnectionErrorArgs&) = delete;

  base::OnceClosure try_again_callback;
};

using AppInstallDialogArgs =
    absl::variant<AppInfoArgs, NoAppErrorArgs, ConnectionErrorArgs>;

}  // namespace ash::app_install

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_ARGS_H_
