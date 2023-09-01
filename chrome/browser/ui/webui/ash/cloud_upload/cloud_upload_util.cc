// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/volume.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::cloud_upload {
namespace {

using file_system_provider::Action;
using file_system_provider::Actions;
using file_system_provider::ProvidedFileSystemInfo;
using file_system_provider::ProvidedFileSystemInterface;
using file_system_provider::ProviderId;
using file_system_provider::Service;

}  // namespace

std::string GetGenericErrorMessage() {
  return l10n_util::GetStringUTF8(IDS_OFFICE_UPLOAD_ERROR_GENERIC);
}

std::string GetReauthenticationRequiredMessage() {
  return l10n_util::GetStringUTF8(
      IDS_OFFICE_UPLOAD_ERROR_REAUTHENTICATION_REQUIRED);
}

storage::FileSystemURL FilePathToFileSystemURL(
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    base::FilePath file_path) {
  GURL url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, file_path, file_manager::util::GetFileManagerURL(), &url)) {
    LOG(ERROR) << "Unable to ConvertAbsoluteFilePathToFileSystemUrl";
    return storage::FileSystemURL();
  }

  return file_system_context->CrackURLInFirstPartyContext(url);
}

SourceType GetSourceType(Profile* profile,
                         const storage::FileSystemURL& source_url) {
  file_manager::VolumeManager* volume_manager =
      file_manager::VolumeManager::Get(profile);
  base::WeakPtr<file_manager::Volume> source_volume =
      volume_manager->FindVolumeFromPath(source_url.path());
  DCHECK(source_volume)
      << "Unable to find source volume (source path filesystem_id: "
      << source_url.filesystem_id() << ")";
  // Local by default.
  if (!source_volume) {
    return SourceType::LOCAL;
  }
  // First, look at whether the filesystem is read-only.
  if (source_volume->is_read_only()) {
    return SourceType::READ_ONLY;
  }
  // Some volume types are generally associated with cloud filesystems.
  if (source_volume->type() == file_manager::VOLUME_TYPE_GOOGLE_DRIVE ||
      source_volume->type() == file_manager::VOLUME_TYPE_SMB ||
      source_volume->type() == file_manager::VOLUME_TYPE_DOCUMENTS_PROVIDER) {
    return SourceType::CLOUD;
  }
  // For provided file systems, check whether file system's source data is
  // retrieved over the network.
  if (source_volume->type() == file_manager::VOLUME_TYPE_PROVIDED) {
    const base::FilePath source_path = source_url.path();
    file_system_provider::Service* service =
        file_system_provider::Service::Get(profile);
    std::vector<file_system_provider::ProvidedFileSystemInfo> file_systems =
        service->GetProvidedFileSystemInfoList();
    for (const auto& file_system : file_systems) {
      if (file_system.mount_path().IsParent(source_path)) {
        return file_system.source() ==
                       extensions::FileSystemProviderSource::SOURCE_NETWORK
                   ? SourceType::CLOUD
                   : SourceType::LOCAL;
      }
    }
    // Local if unable to find the provided file system.
    return SourceType::LOCAL;
  }
  // Local by default.
  return SourceType::LOCAL;
}

UploadType GetUploadType(Profile* profile,
                         const storage::FileSystemURL& source_url) {
  SourceType source_type = GetSourceType(profile, source_url);
  return source_type == SourceType::LOCAL ? UploadType::kMove
                                          : UploadType::kCopy;
}

void RequestODFSMount(Profile* profile,
                      file_system_provider::RequestMountCallback callback) {
  Service* service = Service::Get(profile);
  ProviderId provider_id =
      ProviderId::CreateFromExtensionId(extension_misc::kODFSExtensionId);
  auto logging_callback = base::BindOnce(
      [](file_system_provider::RequestMountCallback callback,
         base::File::Error error) {
        if (error != base::File::FILE_OK) {
          LOG(ERROR) << "RequestMount: " << base::File::ErrorToString(error);
        }
        std::move(callback).Run(error);
      },
      std::move(callback));
  service->RequestMount(provider_id, std::move(logging_callback));
}

absl::optional<ProvidedFileSystemInfo> GetODFSInfo(Profile* profile) {
  Service* service = Service::Get(profile);
  ProviderId provider_id =
      ProviderId::CreateFromExtensionId(extension_misc::kODFSExtensionId);
  auto odfs_infos = service->GetProvidedFileSystemInfoList(provider_id);

  if (odfs_infos.size() == 0) {
    LOG(ERROR) << "ODFS is not mounted";
    return absl::nullopt;
  }
  if (odfs_infos.size() > 1u) {
    LOG(ERROR) << "One and only one filesystem should be mounted for the ODFS "
                  "extension";
    return absl::nullopt;
  }

  return odfs_infos[0];
}

ProvidedFileSystemInterface* GetODFS(Profile* profile) {
  Service* service = Service::Get(profile);
  ProviderId provider_id =
      ProviderId::CreateFromExtensionId(extension_misc::kODFSExtensionId);
  auto odfs_info = GetODFSInfo(profile);
  if (!odfs_info) {
    return nullptr;
  }
  return service->GetProvidedFileSystem(provider_id,
                                        odfs_info->file_system_id());
}

bool IsODFSInstalled(Profile* profile) {
  auto* service = ash::file_system_provider::Service::Get(profile);
  for (const auto& [provider_id, provider] : service->GetProviders()) {
    if (provider_id.GetType() ==
            ash::file_system_provider::ProviderId::EXTENSION &&
        provider_id.GetExtensionId() == extension_misc::kODFSExtensionId) {
      return true;
    }
  }
  return false;
}

bool IsODFSMounted(Profile* profile) {
  // Assume any file system mounted by ODFS is the correct one.
  return GetODFSInfo(profile).has_value();
}

bool IsOfficeWebAppInstalled(Profile* profile) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  bool installed = false;
  proxy->AppRegistryCache().ForOneApp(
      web_app::kMicrosoft365AppId, [&installed](const apps::AppUpdate& update) {
        installed = apps_util::IsInstalled(update.Readiness());
      });
  return installed;
}

// Convert |actions| to |ODFSMetadata| and pass the result to |callback|.
// The action id's for the metadata are HIDDEN_ONEDRIVE_USER_EMAIL and
// HIDDEN_ONEDRIVE_REAUTHENTICATION_REQUIRED.
void OnODFSMetadataActions(GetODFSMetadataCallback callback,
                           const Actions& actions,
                           base::File::Error result) {
  if (result != base::File::Error::FILE_OK) {
    LOG(ERROR) << "Unexpectedly failed to get ODFS metadata actions as these "
                  "should always be returned: "
               << result;
    std::move(callback).Run(base::unexpected(result));
    return;
  }
  ODFSMetadata metadata;
  for (const Action& action : actions) {
    if (action.id == kReauthenticationRequiredId) {
      metadata.reauthentication_required = action.title == "true";
    } else if (action.id == kUserEmailActionId) {
      metadata.user_email = action.title;
    }
  }
  std::move(callback).Run(metadata);
}

void GetODFSMetadata(ProvidedFileSystemInterface* file_system,
                     GetODFSMetadataCallback callback) {
  file_system->GetActions(
      {base::FilePath(cloud_upload::kODFSMetadataQueryPath)},
      base::BindOnce(&OnODFSMetadataActions, std::move(callback)));
}

absl::optional<base::File::Error> GetFirstTaskError(
    const ::file_manager::io_task::ProgressStatus& status) {
  for (const auto* entries : {&status.sources, &status.outputs}) {
    for (const ::file_manager::io_task::EntryStatus& entry : *entries) {
      if (entry.error && *entry.error != base::File::Error::FILE_OK) {
        return entry.error;
      }
    }
  }
  return absl::nullopt;
}

}  // namespace ash::cloud_upload
