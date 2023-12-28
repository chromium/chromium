// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_page_handler.h"

#include <cstddef>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/chromeos/office_web_app/office_web_app.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace ash::cloud_upload {

CloudUploadPageHandler::CloudUploadPageHandler(
    content::WebUI* web_ui,
    Profile* profile,
    mojom::DialogArgsPtr args,
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
    RespondWithUserActionAndCloseCallback user_action_callback,
    RespondWithLocalTaskAndCloseCallback local_task_callback)
    : profile_{profile},
      web_ui_{web_ui},
      dialog_args_{std::move(args)},
      receiver_{this, std::move(pending_page_handler)},
      user_action_callback_{std::move(user_action_callback)},
      local_task_callback_{std::move(local_task_callback)} {}

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
  std::move(callback).Run(dialog_args_ ? dialog_args_.Clone() : nullptr);
}

void CloudUploadPageHandler::IsOfficeWebAppInstalled(
    IsOfficeWebAppInstalledCallback callback) {
  std::move(callback).Run(ash::cloud_upload::IsOfficeWebAppInstalled(profile_));
}

void CloudUploadPageHandler::InstallOfficeWebApp(
    InstallOfficeWebAppCallback callback) {
  auto wrapped_callback = base::BindOnce(
      [](InstallOfficeWebAppCallback callback,
         webapps::InstallResultCode result_code) {
        std::move(callback).Run(webapps::IsSuccess(result_code));
      },
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));

  if (web_app::WebAppProvider::GetForWebApps(profile_)) {
    // Web apps are managed in Ash.
    chromeos::InstallMicrosoft365(profile_, std::move(wrapped_callback));
  } else {
    // Web apps are managed in Lacros.
    crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
        crosapi::CrosapiManager::Get()
            ->crosapi_ash()
            ->web_app_service_ash()
            ->GetWebAppProviderBridge();
    if (!web_app_provider_bridge) {
      std::move(wrapped_callback)
          .Run(webapps::InstallResultCode::kWebAppProviderNotReady);
      return;
    }
    web_app_provider_bridge->InstallMicrosoft365(std::move(wrapped_callback));
  }
}

void CloudUploadPageHandler::IsODFSMounted(IsODFSMountedCallback callback) {
  // Assume any file system mounted by ODFS is the correct one.
  std::move(callback).Run(ash::cloud_upload::IsODFSMounted(profile_));
}

void CloudUploadPageHandler::SignInToOneDrive(
    SignInToOneDriveCallback callback) {
  web_ui_->GetWebContents()->GetTopLevelNativeWindow()->Hide();
  if (!odfs_mount_called_) {
    // Log only once per setup flow.
    odfs_mount_called_ = true;
    UMA_HISTOGRAM_BOOLEAN("FileBrowser.OfficeFiles.Setup.ODFSAvailability",
                          IsODFSInstalled(profile_));
  }
  RequestODFSMount(
      profile_,
      base::BindOnce(&CloudUploadPageHandler::OnMountResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CloudUploadPageHandler::RespondWithUserActionAndClose(
    mojom::UserAction action) {
  if (user_action_callback_) {
    std::move(user_action_callback_).Run(action);
  }
}

void CloudUploadPageHandler::RespondWithLocalTaskAndClose(int task_position) {
  if (local_task_callback_) {
    std::move(local_task_callback_).Run(task_position);
  }
}

void CloudUploadPageHandler::SetOfficeAsDefaultHandler() {
  using file_manager::file_tasks::kActionIdOpenInOffice;

  file_manager::file_tasks::SetWordFileHandlerToFilesSWA(profile_,
                                                         kActionIdOpenInOffice);
  file_manager::file_tasks::SetExcelFileHandlerToFilesSWA(
      profile_, kActionIdOpenInOffice);
  file_manager::file_tasks::SetPowerPointFileHandlerToFilesSWA(
      profile_, kActionIdOpenInOffice);
}

void CloudUploadPageHandler::GetAlwaysMoveOfficeFilesToDrive(
    GetAlwaysMoveOfficeFilesToDriveCallback callback) {
  std::move(callback).Run(
      file_manager::file_tasks::GetAlwaysMoveOfficeFilesToDrive(profile_));
}

void CloudUploadPageHandler::SetAlwaysMoveOfficeFilesToDrive(bool always_move) {
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile_,
                                                            always_move);
}

void CloudUploadPageHandler::GetAlwaysMoveOfficeFilesToOneDrive(
    GetAlwaysMoveOfficeFilesToOneDriveCallback callback) {
  std::move(callback).Run(
      file_manager::file_tasks::GetAlwaysMoveOfficeFilesToOneDrive(profile_));
}

void CloudUploadPageHandler::SetAlwaysMoveOfficeFilesToOneDrive(
    bool always_move) {
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile_,
                                                               always_move);
}

void CloudUploadPageHandler::GetOfficeMoveConfirmationShownForDrive(
    GetOfficeMoveConfirmationShownForDriveCallback callback) {
  std::move(callback).Run(
      file_manager::file_tasks::GetOfficeMoveConfirmationShownForDrive(
          profile_));
}

void CloudUploadPageHandler::GetOfficeMoveConfirmationShownForOneDrive(
    GetOfficeMoveConfirmationShownForOneDriveCallback callback) {
  std::move(callback).Run(
      file_manager::file_tasks::GetOfficeMoveConfirmationShownForOneDrive(
          profile_));
}

void CloudUploadPageHandler::RecordCancel(
    mojom::MetricsRecordedSetupPage page) {
  UMA_HISTOGRAM_ENUMERATION("FileBrowser.OfficeFiles.Setup.CancelPage", page);
}

}  // namespace ash::cloud_upload
