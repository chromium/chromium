// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/files_page/one_drive_page_handler.h"

#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/settings/ash/files_page/mojom/one_drive_handler.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::settings {

namespace {
void OnGetEmailAddress(
    OneDrivePageHandler::GetUserEmailAddressCallback callback,
    const ash::file_system_provider::Actions& actions,
    base::File::Error result) {
  if (result != base::File::Error::FILE_OK) {
    LOG(ERROR) << "Failed to get actions: " << result;
    std::move(callback).Run(absl::nullopt);
    return;
  }
  for (const file_system_provider::Action& action : actions) {
    if (action.id == cloud_upload::kUserEmailActionId) {
      std::move(callback).Run(action.title);
      return;
    }
  }
  std::move(callback).Run(absl::nullopt);
}
}  // namespace

OneDrivePageHandler::OneDrivePageHandler(
    mojo::PendingReceiver<one_drive::mojom::PageHandler> receiver,
    mojo::PendingRemote<one_drive::mojom::Page> page,
    Profile* profile)
    : profile_(profile),
      page_(std::move(page)),
      receiver_(this, std::move(receiver)) {}

OneDrivePageHandler::~OneDrivePageHandler() = default;

void OneDrivePageHandler::GetUserEmailAddress(
    GetUserEmailAddressCallback callback) {
  file_system_provider::Service* service =
      file_system_provider::Service::Get(profile_);
  file_system_provider::ProviderId provider_id =
      file_system_provider::ProviderId::CreateFromExtensionId(
          file_manager::file_tasks::GetODFSExtensionId(profile_));
  std::vector<file_system_provider::ProvidedFileSystemInfo>
      odfs_file_system_infos =
          service->GetProvidedFileSystemInfoList(provider_id);
  if (odfs_file_system_infos.size() == 0) {
    // ODFS is not mounted.
    std::move(callback).Run(absl::nullopt);
    return;
  }
  if (odfs_file_system_infos.size() != 1u) {
    LOG(ERROR) << "One and only one filesystem should be mounted for the ODFS "
                  "extension";
    std::move(callback).Run(absl::nullopt);
    return;
  }
  auto* file_system = service->GetProvidedFileSystem(
      provider_id, odfs_file_system_infos[0].file_system_id());
  file_system->GetActions(
      {base::FilePath("/")},
      base::BindOnce(&OnGetEmailAddress, std::move(callback)));
}

}  // namespace ash::settings
