// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_page_handler.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom.h"
#include "chrome/browser/web_applications/commands/install_from_info_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/webapps/browser/install_result_code.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "url/gurl.h"

namespace ash::cloud_upload {

CloudUploadPageHandler::CloudUploadPageHandler(
    Profile* profile,
    mojom::DialogArgsPtr args,
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
    RespondAndCloseCallback callback)
    : profile_{profile},
      dialog_args_{std::move(args)},
      receiver_{this, std::move(pending_page_handler)},
      callback_{std::move(callback)} {}

CloudUploadPageHandler::~CloudUploadPageHandler() = default;

void CloudUploadPageHandler::GetDialogArgs(GetDialogArgsCallback callback) {
  std::move(callback).Run(dialog_args_ ? dialog_args_.Clone()
                                       : mojom::DialogArgs::New());
}

void CloudUploadPageHandler::IsOfficePWAInstalled(
    IsOfficePWAInstalledCallback callback) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
          profile_)) {
    std::move(callback).Run(false);
    return;
  }
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  bool installed = false;
  proxy->AppRegistryCache().ForOneApp(
      web_app::kMicrosoftOfficeAppId,
      [&installed](const apps::AppUpdate& update) {
        installed = apps_util::IsInstalled(update.Readiness());
      });
  std::move(callback).Run(installed);
}

void CloudUploadPageHandler::RespondAndClose(mojom::UserAction action) {
  if (callback_) {
    std::move(callback_).Run(action);
  }
}

void CloudUploadPageHandler::SetOfficeAsDefaultHandler() {
  using file_manager::file_tasks::kActionIdOpenInOffice;

  file_manager::file_tasks::SetWordFileHandler(profile_, kActionIdOpenInOffice);
  file_manager::file_tasks::SetExcelFileHandler(profile_,
                                                kActionIdOpenInOffice);
  file_manager::file_tasks::SetPowerPointFileHandler(profile_,
                                                     kActionIdOpenInOffice);
  file_manager::file_tasks::SetOfficeSetupComplete(profile_);
}

void CloudUploadPageHandler::SetAlwaysMoveOfficeFiles(bool always_move) {
  file_manager::file_tasks::SetAlwaysMoveOfficeFiles(profile_, always_move);
}

}  // namespace ash::cloud_upload
