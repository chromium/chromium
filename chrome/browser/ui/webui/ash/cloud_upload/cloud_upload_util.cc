// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

#include <optional>

#include "ash/constants/web_app_id_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"

namespace ash::cloud_upload {
namespace {

using file_system_provider::Action;
using file_system_provider::Actions;
using file_system_provider::ProvidedFileSystemInfo;
using file_system_provider::ProvidedFileSystemInterface;
using file_system_provider::ProviderId;
using file_system_provider::Service;

}  // namespace

ODFSEntryMetadata::ODFSEntryMetadata() = default;
ODFSEntryMetadata::ODFSEntryMetadata(const ODFSEntryMetadata&) = default;
ODFSEntryMetadata::~ODFSEntryMetadata() = default;

std::string GetGenericErrorMessage() {
  return l10n_util::GetStringUTF8(IDS_OFFICE_UPLOAD_ERROR_GENERIC);
}

std::string GetReauthenticationRequiredMessage() {
  return l10n_util::GetStringUTF8(
      IDS_OFFICE_UPLOAD_ERROR_REAUTHENTICATION_REQUIRED);
}

std::string GetNotAValidDocumentErrorMessage() {
  return l10n_util::GetStringUTF8(IDS_OFFICE_UPLOAD_ERROR_NOT_A_VALID_DOCUMENT);
}

std::string GetAlreadyBeingOpenedMessage() {
  return l10n_util::GetStringUTF8(IDS_OFFICE_FILE_ALREADY_BEING_OPENED_MESSAGE);
}

std::string GetAlreadyBeingOpenedTitle() {
  return l10n_util::GetStringUTF8(IDS_OFFICE_FILE_ALREADY_BEING_OPENED_TITLE);
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

OfficeFilesSourceVolume VolumeTypeToSourceVolume(
    file_manager::VolumeType volume_type) {
  switch (volume_type) {
    case file_manager::VOLUME_TYPE_GOOGLE_DRIVE:
      return OfficeFilesSourceVolume::kGoogleDrive;
    case file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY:
      return OfficeFilesSourceVolume::kDownloadsDirectory;
    case file_manager::VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
      return OfficeFilesSourceVolume::kRemovableDiskPartition;
    case file_manager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
      return OfficeFilesSourceVolume::kMountedArchiveFile;
    case file_manager::VOLUME_TYPE_PROVIDED:
      return OfficeFilesSourceVolume::kProvided;
    case file_manager::VOLUME_TYPE_MTP:
      return OfficeFilesSourceVolume::kMtp;
    case file_manager::VOLUME_TYPE_MEDIA_VIEW:
      return OfficeFilesSourceVolume::kMediaView;
    case file_manager::VOLUME_TYPE_CROSTINI:
      return OfficeFilesSourceVolume::kCrostini;
    case file_manager::VOLUME_TYPE_ANDROID_FILES:
      return OfficeFilesSourceVolume::kAndriodFiles;
    case file_manager::VOLUME_TYPE_DOCUMENTS_PROVIDER:
      return OfficeFilesSourceVolume::kDocumentsProvider;
    case file_manager::VOLUME_TYPE_SMB:
      return OfficeFilesSourceVolume::kSmb;
    case file_manager::VOLUME_TYPE_SYSTEM_INTERNAL:
      return OfficeFilesSourceVolume::kSystemInternal;
    case file_manager::VOLUME_TYPE_GUEST_OS:
      return OfficeFilesSourceVolume::kGuestOS;
    // TODO(b/304383409): remove default class after making VolumeType an enum
    // class.
    default:
      LOG(ERROR) << "Unknown VolumeType " << volume_type;
      return OfficeFilesSourceVolume::kUnknown;
  }
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
    LOG(ERROR) << "Unable to find source volume (source path filesystem_id: "
               << source_url.filesystem_id() << ")";
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

std::optional<ProvidedFileSystemInfo> GetODFSInfo(Profile* profile) {
  Service* service = Service::Get(profile);
  ProviderId provider_id =
      ProviderId::CreateFromExtensionId(extension_misc::kODFSExtensionId);
  auto odfs_infos = service->GetProvidedFileSystemInfoList(provider_id);

  if (odfs_infos.size() == 0) {
    return std::nullopt;
  }
  if (odfs_infos.size() > 1u) {
    LOG(ERROR) << "One and only one filesystem should be mounted for the ODFS "
                  "extension";
    return std::nullopt;
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

base::FilePath GetODFSFuseboxMount(Profile* profile) {
  const auto odfs_info = GetODFSInfo(profile);
  if (!odfs_info) {
    return base::FilePath();
  }

  file_manager::VolumeManager* volume_manager =
      file_manager::VolumeManager::Get(profile);
  if (!volume_manager) {
    return base::FilePath();
  }

  for (const auto& volume : volume_manager->GetVolumeList()) {
    if (volume->volume_label() == odfs_info->display_name() &&
        volume->file_system_type() == file_manager::util::kFuseBox) {
      return volume->mount_path();
    }
  }
  return base::FilePath();
}

bool IsODFSInstalled(Profile* profile) {
  auto* service = ash::file_system_provider::Service::Get(profile);
  return base::ranges::any_of(
      service->GetProviders(), [](const auto& provider) {
        return provider.first ==
               ash::file_system_provider::ProviderId::CreateFromExtensionId(
                   extension_misc::kODFSExtensionId);
      });
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

bool IsMicrosoftOfficeOneDriveIntegrationAllowedAndOdfsInstalled(
    Profile* profile) {
  return chromeos::cloud_upload::IsMicrosoftOfficeOneDriveIntegrationAllowed(
             profile) &&
         IsODFSInstalled(profile);
}

bool UrlIsOnODFS(const FileSystemURL& url) {
  ash::file_system_provider::util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    return false;
  }

  file_system_provider::ProviderId provider_id =
      file_system_provider::ProviderId::CreateFromExtensionId(
          extension_misc::kODFSExtensionId);
  if (parser.file_system()->GetFileSystemInfo().provider_id() != provider_id) {
    return false;
  }
  return true;
}

// Convert |actions| to |ODFSMetadata| and pass the result to |callback|.
// The action id's for the metadata are HIDDEN_ONEDRIVE_USER_EMAIL,
// HIDDEN_ONEDRIVE_REAUTHENTICATION_REQUIRED and HIDDEN_ONEDRIVE_ACCOUNT_STATE.
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
    } else if (action.id == kAccountStateId) {
      if (action.title == "NORMAL") {
        metadata.account_state = OdfsAccountState::kNormal;
      } else if (action.title == "REAUTHENTICATION_REQUIRED") {
        metadata.account_state = OdfsAccountState::kReauthenticationRequired;
      } else if (action.title == "FROZEN_ACCOUNT") {
        metadata.account_state = OdfsAccountState::kFrozenAccount;
      }
    } else if (action.id == kUserEmailActionId) {
      metadata.user_email = action.title;
    }
  }
  std::move(callback).Run(metadata);
}

// Convert ODFS-specific entry metadata returned in `actions` to
// `ODFSEntryMetadata`.
void OnGetODFSEntryActions(GetODFSEntryMetadataCallback callback,
                           const Actions& actions,
                           base::File::Error result) {
  if (result != base::File::Error::FILE_OK) {
    std::move(callback).Run(base::unexpected(result));
    return;
  }
  ODFSEntryMetadata metadata;
  for (const Action& action : actions) {
    if (action.id == kOneDriveUrlActionId) {
      // Custom actions are used to pass a OneDrive document URLs as the "title"
      // attribute.
      metadata.url = action.title;
    }
  }
  std::move(callback).Run(std::move(metadata));
}

void GetODFSMetadata(ProvidedFileSystemInterface* file_system,
                     GetODFSMetadataCallback callback) {
  file_system->GetActions(
      {base::FilePath(cloud_upload::kODFSMetadataQueryPath)},
      base::BindOnce(&OnODFSMetadataActions, std::move(callback)));
}

void GetODFSEntryMetadata(
    file_system_provider::ProvidedFileSystemInterface* file_system,
    const base::FilePath& path,
    GetODFSEntryMetadataCallback callback) {
  file_system->GetActions(
      {path}, base::BindOnce(&OnGetODFSEntryActions, std::move(callback)));
}

bool PathIsOnDriveFS(Profile* profile, const base::FilePath& file_path) {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  base::FilePath relative_path;
  return integration_service->GetRelativeDrivePath(file_path, &relative_path);
}

std::optional<base::File::Error> GetFirstTaskError(
    const ::file_manager::io_task::ProgressStatus& status) {
  for (const auto* entries : {&status.sources, &status.outputs}) {
    for (const ::file_manager::io_task::EntryStatus& entry : *entries) {
      if (entry.error && *entry.error != base::File::Error::FILE_OK) {
        return entry.error;
      }
    }
  }
  return std::nullopt;
}

std::optional<gfx::Rect> CalculateAuthWindowBounds(Profile* profile) {
  Browser* browser =
      FindSystemWebAppBrowser(profile, ash::SystemWebAppType::FILE_MANAGER);
  if (!browser) {
    return std::nullopt;
  }

  gfx::Rect files_app_bounds = browser->window()->GetBounds();
  // These are the min sizes needed for the oauth dialog to look subjectively
  // "good".
  const int kMinWidth = 615;
  const int kMinHeight = 660;
  // The dialog won't fit inside Files app's bounds, but we'll try and keep it
  // centered around the same point.
  if (files_app_bounds.width() < kMinWidth ||
      files_app_bounds.height() < kMinHeight) {
    int files_app_center_x =
        files_app_bounds.x() + files_app_bounds.width() / 2;
    int files_app_center_y =
        files_app_bounds.y() + files_app_bounds.height() / 2;
    int target_x = std::max(0, files_app_center_x - kMinWidth / 2);
    int target_y = std::max(0, files_app_center_y - kMinHeight / 2);
    return gfx::Rect(target_x, target_y, kMinWidth, kMinHeight);
  }

  // Files app is bigger in both dimensions - shrink popup to min sizes and keep
  // it centered.
  gfx::Rect popup_bounds(files_app_bounds);
  popup_bounds.ClampToCenteredSize(gfx::Size(kMinWidth, kMinHeight));
  return popup_bounds;
}

}  // namespace ash::cloud_upload
