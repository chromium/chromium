// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_file_suggester.h"

#include <optional>
#include <string>
#include <utility>

#include "base/barrier_callback.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/files/file_title.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_model.h"
#include "chrome/browser/ash/fileapi/recent_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"

namespace {

using LocalFile = PickerFileSuggester::LocalFile;
using DriveFile = PickerFileSuggester::DriveFile;
namespace fmp = extensions::api::file_manager_private;

constexpr base::TimeDelta kMaxDriveFileRecencyDelta = base::Days(30);
constexpr base::TimeDelta kScanTimeout = base::Seconds(1);

storage::FileSystemContext* GetFileSystemContextForProfile(Profile* profile) {
  content::StoragePartition* storage = profile->GetDefaultStoragePartition();
  return storage->GetFileSystemContext();
}

void GetRecentFiles(Profile* profile,
                    ash::RecentSource::FileType file_type,
                    fmp::VolumeType volume_type,
                    size_t max_files,
                    base::TimeDelta now_delta,
                    ash::RecentModel::GetRecentFilesCallback callback) {
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      GetFileSystemContextForProfile(profile);
  if (!file_system_context) {
    return;
  }

  ash::RecentModel* model = ash::RecentModelFactory::GetForProfile(profile);
  if (!model) {
    return;
  }
  ash::RecentModelOptions options;
  options.now_delta = now_delta;
  options.file_type = file_type;
  options.scan_timeout = kScanTimeout;
  options.max_files = max_files;
  options.source_specs = {
      ash::RecentSourceSpec{.volume_type = volume_type},
  };

  model->GetRecentFiles(file_system_context.get(), GURL(), /*query=*/"",
                        options, std::move(callback));
}

void GetDriveFileMetadata(
    drive::DriveIntegrationService* drive_integration,
    const ash::RecentFile& file,
    base::OnceCallback<void(std::optional<DriveFile>)> callback) {
  const storage::FileSystemURL& url = file.url();
  if (url.type() != storage::kFileSystemTypeDriveFs) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const base::FilePath& path = url.path();
  drive_integration->GetMetadata(
      path, base::BindOnce(
                [](const base::FilePath& path, drive::FileError error,
                   drivefs::mojom::FileMetadataPtr metadata)
                    -> std::optional<DriveFile> {
                  if (error != drive::FILE_ERROR_OK) {
                    return std::nullopt;
                  }
                  return DriveFile(metadata->item_id,
                                   app_list::GetFileTitle(path), path,
                                   GURL(metadata->alternate_url));
                },
                path)
                .Then(std::move(callback)));
}

std::vector<DriveFile> FilterDriveFiles(
    std::vector<std::optional<DriveFile>> files) {
  std::vector<DriveFile> filtered;
  filtered.reserve(files.size());
  for (std::optional<DriveFile>& file : files) {
    if (file.has_value()) {
      filtered.push_back(std::move(*file));
    }
  }
  filtered.shrink_to_fit();
  return filtered;
}

}  // namespace

PickerFileSuggester::DriveFile::DriveFile(std::optional<std::string> id,
                                          std::u16string title,
                                          base::FilePath local_path,
                                          GURL url)
    : id(std::move(id)),
      title(std::move(title)),
      local_path(std::move(local_path)),
      url(std::move(url)) {}

PickerFileSuggester::DriveFile::~DriveFile() = default;

PickerFileSuggester::DriveFile::DriveFile(const DriveFile&) = default;
PickerFileSuggester::DriveFile::DriveFile(DriveFile&&) = default;
PickerFileSuggester::DriveFile& PickerFileSuggester::DriveFile::operator=(
    const DriveFile&) = default;
PickerFileSuggester::DriveFile& PickerFileSuggester::DriveFile::operator=(
    DriveFile&&) = default;

PickerFileSuggester::PickerFileSuggester(Profile* profile)
    : profile_(profile) {}

PickerFileSuggester::~PickerFileSuggester() = default;

void PickerFileSuggester::GetRecentLocalImages(
    size_t max_files,
    base::TimeDelta now_delta,
    RecentLocalImagesCallback callback) {
  GetRecentFiles(
      profile_, ash::RecentSource::FileType::kImage,
      fmp::VolumeType::kDownloads, max_files, now_delta,
      base::BindOnce(&PickerFileSuggester::OnGetRecentLocalImages,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PickerFileSuggester::GetRecentDriveFiles(
    size_t max_files,
    RecentDriveFilesCallback callback) {
  GetRecentFiles(
      profile_, ash::RecentSource::FileType::kAll, fmp::VolumeType::kDrive,
      max_files, kMaxDriveFileRecencyDelta,
      base::BindOnce(&PickerFileSuggester::OnGetRecentDriveFiles,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PickerFileSuggester::OnGetRecentLocalImages(
    RecentLocalImagesCallback callback,
    const std::vector<ash::RecentFile>& recent_files) {
  std::vector<LocalFile> files;
  files.reserve(recent_files.size());
  for (const ash::RecentFile& recent_file : recent_files) {
    const base::FilePath& path = recent_file.url().path();
    files.push_back({.title = app_list::GetFileTitle(path), .path = path});
  }
  std::move(callback).Run(std::move(files));
}

void PickerFileSuggester::OnGetRecentDriveFiles(
    RecentDriveFilesCallback callback,
    const std::vector<ash::RecentFile>& recent_files) {
  drive::DriveIntegrationService* drive_integration =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (!drive_integration) {
    std::move(callback).Run({});
  }

  auto barrier_callback = base::BarrierCallback<std::optional<DriveFile>>(
      /*num_callbacks=*/recent_files.size(),
      /*done_callback=*/base::BindOnce(FilterDriveFiles)
          .Then(std::move(callback)));

  for (const ash::RecentFile& recent_file : recent_files) {
    GetDriveFileMetadata(drive_integration, recent_file, barrier_callback);
  }
}
