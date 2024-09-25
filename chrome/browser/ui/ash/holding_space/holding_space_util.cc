// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/image_util.h"
#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/thumbnail_loader/thumbnail_loader.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "storage/browser/file_system/file_system_context.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace ash {
namespace holding_space_util {
namespace {

// Helpers ---------------------------------------------------------------------

HoldingSpaceFile::FileSystemType ToHoldingSpaceFileSystemType(
    storage::FileSystemType file_system_type) {
  switch (file_system_type) {
    case storage::FileSystemType::kFileSystemTypeArcContent:
      return HoldingSpaceFile::FileSystemType::kArcContent;
    case storage::FileSystemType::kFileSystemTypeArcDocumentsProvider:
      return HoldingSpaceFile::FileSystemType::kArcDocumentsProvider;
    case storage::FileSystemType::kFileSystemTypeDeviceMedia:
      return HoldingSpaceFile::FileSystemType::kDeviceMedia;
    case storage::FileSystemType::kFileSystemTypeDeviceMediaAsFileStorage:
      return HoldingSpaceFile::FileSystemType::kDeviceMediaAsFileStorage;
    case storage::FileSystemType::kFileSystemTypeDragged:
      return HoldingSpaceFile::FileSystemType::kDragged;
    case storage::FileSystemType::kFileSystemTypeDriveFs:
      return HoldingSpaceFile::FileSystemType::kDriveFs;
    case storage::FileSystemType::kFileSystemTypeExternal:
      return HoldingSpaceFile::FileSystemType::kExternal;
    case storage::FileSystemType::kFileSystemTypeForTransientFile:
      return HoldingSpaceFile::FileSystemType::kForTransientFile;
    case storage::FileSystemType::kFileSystemTypeFuseBox:
      return HoldingSpaceFile::FileSystemType::kFuseBox;
    case storage::FileSystemType::kFileSystemTypeIsolated:
      return HoldingSpaceFile::FileSystemType::kIsolated;
    case storage::FileSystemType::kFileSystemTypeLocal:
      return HoldingSpaceFile::FileSystemType::kLocal;
    case storage::FileSystemType::kFileSystemTypeLocalForPlatformApp:
      return HoldingSpaceFile::FileSystemType::kLocalForPlatformApp;
    case storage::FileSystemType::kFileSystemTypeLocalMedia:
      return HoldingSpaceFile::FileSystemType::kLocalMedia;
    case storage::FileSystemType::kFileSystemTypePersistent:
      return HoldingSpaceFile::FileSystemType::kPersistent;
    case storage::FileSystemType::kFileSystemTypeProvided:
      return HoldingSpaceFile::FileSystemType::kProvided;
    case storage::FileSystemType::kFileSystemTypeSmbFs:
      return HoldingSpaceFile::FileSystemType::kSmbFs;
    case storage::FileSystemType::kFileSystemTypeSyncable:
      return HoldingSpaceFile::FileSystemType::kSyncable;
    case storage::FileSystemType::kFileSystemTypeSyncableForInternalSync:
      return HoldingSpaceFile::FileSystemType::kSyncableForInternalSync;
    case storage::FileSystemType::kFileSystemTypeTemporary:
      return HoldingSpaceFile::FileSystemType::kTemporary;
    case storage::FileSystemType::kFileSystemTypeTest:
      return HoldingSpaceFile::FileSystemType::kTest;
    case storage::FileSystemType::kFileSystemTypeUnknown:
      return HoldingSpaceFile::FileSystemType::kUnknown;
    case storage::FileSystemType::kFileSystemInternalTypeEnumStart:
    case storage::FileSystemType::kFileSystemInternalTypeEnumEnd:
      NOTREACHED();
  }
}

}  // namespace

// ValidityRequirement ---------------------------------------------------------

ValidityRequirement::ValidityRequirement() = default;
ValidityRequirement::ValidityRequirement(const ValidityRequirement&) = default;
ValidityRequirement::ValidityRequirement(ValidityRequirement&& other) = default;

// Utilities -------------------------------------------------------------------

bool ShouldSkipPathCheck(Profile* profile, const base::FilePath& path) {
  // Drive FS may be in the middle of restarting, so if it is enabled but not
  // mounted, assume any files in drive are valid.
  const auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  return drive_integration_service && drive_integration_service->is_enabled() &&
         !drive_integration_service->IsMounted() &&
         drive_integration_service->GetMountPointPath().IsParent(path);
}

void FilePathValid(Profile* profile,
                   FilePathWithValidityRequirement file_path_with_requirement,
                   FilePathValidCallback callback) {
  auto* user = ProfileHelper::Get()->GetUserByProfile(profile);
  file_manager::util::GetMetadataForPath(
      file_manager::util::GetFileManagerFileSystemContext(profile),
      file_path_with_requirement.first,
      // NOTE: Provided file systems DCHECK if no metadata field is requested.
      // TODO(http://b/274011452): Investigate if provided file systems should
      //                           be supported by holding space.
      // TODO(http://b/274011722): Investigate if we can remove time based
      //                           validation of items in holding space.
      {storage::FileSystemOperation::GetMetadataField::kLastModified},
      base::BindOnce(
          [](FilePathValidCallback callback,
             FilePathWithValidityRequirement file_path_with_requirement,
             AccountId account_id, base::File::Error result,
             const base::File::Info& file_info) {
            Profile* profile =
                ProfileHelper::Get()->GetProfileByAccountId(account_id);
            if (profile && ShouldSkipPathCheck(
                               profile, file_path_with_requirement.first)) {
              std::move(callback).Run(true);
              return;
            }

            bool valid = true;
            const ValidityRequirement& requirement =
                file_path_with_requirement.second;
            if (requirement.must_exist)
              valid = result == base::File::Error::FILE_OK;
            if (valid && requirement.must_be_newer_than) {
              valid =
                  file_info.creation_time >
                  base::Time::Now() - requirement.must_be_newer_than.value();
            }
            std::move(callback).Run(valid);
          },
          std::move(callback), file_path_with_requirement,
          user ? user->GetAccountId() : EmptyAccountId()));
}

void PartitionFilePathsByExistence(
    Profile* profile,
    FilePathList file_paths,
    PartitionFilePathsByExistenceCallback callback) {
  FilePathsWithValidityRequirements file_paths_with_requirements;
  for (const auto& file_path : file_paths)
    file_paths_with_requirements.push_back({file_path, /*requirements=*/{}});
  PartitionFilePathsByValidity(profile, file_paths_with_requirements,
                               std::move(callback));
}

void PartitionFilePathsByValidity(
    Profile* profile,
    FilePathsWithValidityRequirements file_paths_with_requirements,
    PartitionFilePathsByValidityCallback callback) {
  if (file_paths_with_requirements.empty()) {
    std::move(callback).Run(/*valid_file_paths=*/{},
                            /*invalid_file_paths=*/{});
    return;
  }

  auto valid_file_paths = std::make_unique<FilePathList>();
  auto invalid_file_paths = std::make_unique<FilePathList>();

  auto* valid_file_paths_ptr = valid_file_paths.get();
  auto* invalid_file_paths_ptr = invalid_file_paths.get();

  FilePathList file_paths;
  for (const auto& file_path_with_requirement : file_paths_with_requirements)
    file_paths.push_back(file_path_with_requirement.first);

  // This `barrier_closure` will be run after verifying the existence of all
  // `file_paths`. It is expected that both `valid_file_paths` and
  // `invalid_file_paths` will have been populated by the time of
  // invocation.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      file_paths_with_requirements.size(),
      base::BindOnce(
          [](FilePathList sorted_file_paths,
             std::unique_ptr<FilePathList> valid_file_paths,
             std::unique_ptr<FilePathList> invalid_file_paths,
             PartitionFilePathsByValidityCallback callback) {
            // We need to sort our partitioned vectors to match the original
            // order that was provided at call time. This is necessary as the
            // original order may have been lost due to race conditions when
            // checking for file path existence.
            auto sort = [&sorted_file_paths](FilePathList* file_paths) {
              FilePathList temp_file_paths;
              temp_file_paths.swap(*file_paths);
              for (const auto& file_path : sorted_file_paths) {
                if (base::Contains(temp_file_paths, file_path))
                  file_paths->push_back(file_path);
              }
            };
            sort(valid_file_paths.get());
            sort(invalid_file_paths.get());

            // Ownership of the partitioned vectors is passed to `callback`.
            std::move(callback).Run(std::move(*valid_file_paths),
                                    std::move(*invalid_file_paths));
          },
          /*sorted_file_paths=*/file_paths, std::move(valid_file_paths),
          std::move(invalid_file_paths), std::move(callback)));

  // Verify existence of each `file_path`. Upon successful check of existence,
  // each `file_path` should be pushed into either `valid_file_paths` or
  // `invalid_file_paths` as appropriate.
  for (const auto& file_path_with_requirement : file_paths_with_requirements) {
    FilePathValid(
        profile, file_path_with_requirement,
        base::BindOnce(
            [](base::FilePath file_path, FilePathList* valid_file_paths,
               FilePathList* invalid_file_paths,
               base::RepeatingClosure barrier_closure, bool exists) {
              if (exists)
                valid_file_paths->push_back(file_path);
              else
                invalid_file_paths->push_back(file_path);
              barrier_closure.Run();
            },
            file_path_with_requirement.first,
            base::Unretained(valid_file_paths_ptr),
            base::Unretained(invalid_file_paths_ptr), barrier_closure));
  }
}

HoldingSpaceFile::FileSystemType ResolveFileSystemType(
    Profile* profile,
    const GURL& file_system_url) {
  return ToHoldingSpaceFileSystemType(
      file_manager::util::GetFileManagerFileSystemContext(profile)
          ->CrackURLInFirstPartyContext(file_system_url)
          .type());
}

GURL ResolveFileSystemUrl(Profile* profile, const base::FilePath& file_path) {
  GURL file_system_url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, file_path, file_manager::util::GetFileManagerURL(),
          &file_system_url)) {
    VLOG(2) << "Unable to convert file path to File System URL.";
  }
  return file_system_url;
}

std::unique_ptr<HoldingSpaceImage> ResolveImage(
    ThumbnailLoader* thumbnail_loader,
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return ResolveImageWithPlaceholderImageSkiaResolver(
      thumbnail_loader,
      /*placeholder_image_skia_resolver=*/base::NullCallback(), type,
      file_path);
}

std::unique_ptr<HoldingSpaceImage> ResolveImageWithPlaceholderImageSkiaResolver(
    ThumbnailLoader* thumbnail_loader,
    HoldingSpaceImage::PlaceholderImageSkiaResolver
        placeholder_image_skia_resolver,
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return std::make_unique<HoldingSpaceImage>(
      GetMaxImageSizeForType(type), file_path,
      /*async_bitmap_resolver=*/
      base::BindRepeating(
          [](const base::WeakPtr<ThumbnailLoader>& thumbnail_loader,
             const base::FilePath& file_path, const gfx::Size& size,
             HoldingSpaceImage::BitmapCallback callback) {
            if (thumbnail_loader)
              thumbnail_loader->Load({file_path, size}, std::move(callback));
          },
          thumbnail_loader->GetWeakPtr()),
      /*placeholder_image_skia_resolver=*/
      base::BindRepeating(
          [](HoldingSpaceImage::PlaceholderImageSkiaResolver
                 placeholder_image_skia_resolver,
             const base::FilePath& file_path, const gfx::Size& size,
             const std::optional<bool>& dark_background,
             const std::optional<bool>& is_folder) {
            // When the initial placeholder is being created during
            // construction, `dark_background` and `is_folder` will be absent.
            // In that case, don't show a placeholder to minimize jank.
            if (!dark_background.has_value() && !is_folder.has_value())
              return image_util::CreateEmptyImage(size);
            // If an explicit `placeholder_image_skia_resolver` has been
            // specified, use it to create the appropriate placeholder image.
            if (!placeholder_image_skia_resolver.is_null()) {
              return placeholder_image_skia_resolver.Run(
                  file_path, size, dark_background, is_folder);
            }
            // Otherwise, fallback to default behavior which is to create an
            // image corresponding to the file type of the associated backing
            // file.
            return HoldingSpaceImage::
                CreateDefaultPlaceholderImageSkiaResolver()
                    .Run(file_path, size, dark_background, is_folder);
          },
          placeholder_image_skia_resolver));
}

}  // namespace holding_space_util
}  // namespace ash
