// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/files/one_drive_page_handler.h"

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/mojom/one_drive_handler.mojom.h"
#include "chrome/common/extensions/extension_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::settings {

namespace {
void OnGetEmailAddress(
    OneDrivePageHandler::GetUserEmailAddressCallback callback,
    base::expected<cloud_upload::ODFSMetadata, base::File::Error>
        metadata_or_error) {
  if (!metadata_or_error.has_value()) {
    LOG(ERROR) << "Failed to get user email: " << metadata_or_error.error();
    std::move(callback).Run(absl::nullopt);
    return;
  }
  if (metadata_or_error->user_email.empty()) {
    LOG(ERROR) << "User email is empty";
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(metadata_or_error->user_email);
}

void OnShowItemInFolder(
    OneDrivePageHandler::OpenOneDriveFolderCallback callback,
    platform_util::OpenOperationResult result) {
  std::move(callback).Run(result ==
                          platform_util::OpenOperationResult::OPEN_SUCCEEDED);
}
}  // namespace

OneDrivePageHandler::OneDrivePageHandler(
    mojo::PendingReceiver<one_drive::mojom::PageHandler> receiver,
    mojo::PendingRemote<one_drive::mojom::Page> page,
    Profile* profile)
    : profile_(profile),
      page_(std::move(page)),
      receiver_(this, std::move(receiver)) {
  file_system_provider::Service* service =
      file_system_provider::Service::Get(profile_);
  if (service) {
    service->AddObserver(this);
  }
}

OneDrivePageHandler::~OneDrivePageHandler() {
  file_system_provider::Service* service =
      file_system_provider::Service::Get(profile_);
  if (service) {
    service->RemoveObserver(this);
  }
}

void OneDrivePageHandler::GetUserEmailAddress(
    GetUserEmailAddressCallback callback) {
  file_system_provider::ProvidedFileSystemInterface* file_system =
      cloud_upload::GetODFS(profile_);
  if (!file_system) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  cloud_upload::GetODFSMetadata(
      file_system, base::BindOnce(&OnGetEmailAddress, std::move(callback)));
}

void OneDrivePageHandler::ConnectToOneDrive(
    ConnectToOneDriveCallback callback) {
  absl::optional<file_system_provider::ProvidedFileSystemInfo>
      odfs_file_system_info = cloud_upload::GetODFSInfo(profile_);
  if (odfs_file_system_info.has_value()) {
    // ODFS is already mounted.
    std::move(callback).Run(false);
    return;
  }
  // Show connect OneDrive dialog. This method's callback is called before the
  // user tries to sign in. The connection status is detected separately by
  // listening to provided file system mount events.
  Browser* browser =
      FindSystemWebAppBrowser(profile_, ash::SystemWebAppType::FILE_MANAGER);
  gfx::NativeWindow modal_parent =
      browser ? browser->window()->GetNativeWindow() : nullptr;
  std::move(callback).Run(
      ash::cloud_upload::ShowConnectOneDriveDialog(modal_parent));
}

void OneDrivePageHandler::DisconnectFromOneDrive(
    DisconnectFromOneDriveCallback callback) {
  absl::optional<file_system_provider::ProvidedFileSystemInfo> file_system =
      cloud_upload::GetODFSInfo(profile_);
  if (!file_system) {
    // ODFS is not mounted.
    std::move(callback).Run(false);
    return;
  }
  auto* service = file_system_provider::Service::Get(profile_);
  std::move(callback).Run(service->RequestUnmount(
      file_system->provider_id(), file_system->file_system_id()));
}

void OneDrivePageHandler::OpenOneDriveFolder(
    OpenOneDriveFolderCallback callback) {
  absl::optional<file_system_provider::ProvidedFileSystemInfo> file_system =
      cloud_upload::GetODFSInfo(profile_);
  if (!file_system) {
    // ODFS is not mounted.
    std::move(callback).Run(false);
    return;
  }
  file_manager::util::ShowItemInFolder(
      profile_, file_system->mount_path(),
      base::BindOnce(&OnShowItemInFolder, std::move(callback)));
}

void OneDrivePageHandler::OnProvidedFileSystemMount(
    const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
    ash::file_system_provider::MountContext context,
    base::File::Error error) {
  file_system_provider::ProviderId odfs_provider_id =
      file_system_provider::ProviderId::CreateFromExtensionId(
          extension_misc::kODFSExtensionId);
  // Only observe successful mount events for ODFS.
  if (file_system_info.provider_id() != odfs_provider_id ||
      error != base::File::FILE_OK) {
    return;
  }
  page_->OnODFSMountOrUnmount();
}

void OneDrivePageHandler::OnProvidedFileSystemUnmount(
    const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
    base::File::Error error) {
  file_system_provider::ProviderId odfs_provider_id =
      file_system_provider::ProviderId::CreateFromExtensionId(
          extension_misc::kODFSExtensionId);
  // Only observe successful unmount events for ODFS.
  if (file_system_info.provider_id() != odfs_provider_id ||
      error != base::File::FILE_OK) {
    return;
  }
  page_->OnODFSMountOrUnmount();
}

}  // namespace ash::settings
