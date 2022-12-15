// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_page_handler.h"

#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/chromeos/office_web_app/office_web_app.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/webapps/browser/install_result_code.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace ash::cloud_upload {

using ash::file_system_provider::ProvidedFileSystemInfo;
using ash::file_system_provider::ProviderId;
using ash::file_system_provider::Service;

CloudUploadPageHandler::CloudUploadPageHandler(
    content::WebUI* web_ui,
    Profile* profile,
    mojom::DialogArgsPtr args,
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
    RespondAndCloseCallback callback)
    : profile_{profile},
      web_ui_{web_ui},
      dialog_args_{std::move(args)},
      receiver_{this, std::move(pending_page_handler)},
      callback_{std::move(callback)} {}

CloudUploadPageHandler::~CloudUploadPageHandler() = default;

void CloudUploadPageHandler::OnMountResponse(
    CloudUploadPageHandler::SignInToOneDriveCallback callback,
    base::File::Error result) {
  gfx::NativeWindow window =
      web_ui_->GetWebContents()->GetTopLevelNativeWindow();
  window->Show();
  window->Focus();
  std::move(callback).Run(result == base::File::FILE_OK);
}

void CloudUploadPageHandler::GetDialogArgs(GetDialogArgsCallback callback) {
  std::move(callback).Run(dialog_args_ ? dialog_args_.Clone()
                                       : mojom::DialogArgs::New());
}

void CloudUploadPageHandler::IsOfficeWebAppInstalled(
    IsOfficeWebAppInstalledCallback callback) {
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

void CloudUploadPageHandler::InstallOfficeWebApp(
    InstallOfficeWebAppCallback callback) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
  if (provider == nullptr) {
    // TODO(b/259869338): This means that web apps are managed in Lacros, so we
    // need to add a crosapi to install the web app.
    std::move(callback).Run(false);
    return;
  }

  auto wrapped_callback = base::BindOnce(
      [](InstallOfficeWebAppCallback callback,
         webapps::InstallResultCode result_code) {
        std::move(callback).Run(webapps::IsSuccess(result_code));
      },
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));

  // Web apps are managed in Ash.
  chromeos::InstallMicrosoft365(profile_, std::move(wrapped_callback));
}

void CloudUploadPageHandler::IsODFSMounted(IsODFSMountedCallback callback) {
  Service* service = Service::Get(profile_);
  ProviderId provider_id = ProviderId::CreateFromExtensionId(
      file_manager::file_tasks::kODFSExtensionId);
  std::vector<ProvidedFileSystemInfo> file_systems =
      service->GetProvidedFileSystemInfoList(provider_id);

  // Assume any file system mounted by ODFS is the correct one.
  std::move(callback).Run(!file_systems.empty());
}

void CloudUploadPageHandler::SignInToOneDrive(
    SignInToOneDriveCallback callback) {
  Service* service = Service::Get(profile_);
  ProviderId provider_id = ProviderId::CreateFromExtensionId(
      file_manager::file_tasks::kODFSExtensionId);
  web_ui_->GetWebContents()->GetTopLevelNativeWindow()->Hide();
  service->RequestMount(
      provider_id,
      base::BindOnce(&CloudUploadPageHandler::OnMountResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
