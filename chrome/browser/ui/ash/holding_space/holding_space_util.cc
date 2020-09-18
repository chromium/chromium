// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"

#include "ash/public/cpp/file_icon_util.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_thumbnail_loader.h"
#include "storage/browser/file_system/file_system_context.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace ash {
namespace holding_space_util {

namespace {

base::Optional<base::Time> now_for_testing;

// Helpers ---------------------------------------------------------------------

gfx::ImageSkia GetPlaceholderImage(HoldingSpaceItem::Type type,
                                   const base::FilePath& file_path) {
  gfx::Size size;
  switch (type) {
    case HoldingSpaceItem::Type::kDownload:
    case HoldingSpaceItem::Type::kPinnedFile:
      size = gfx::Size(kHoldingSpaceChipIconSize, kHoldingSpaceChipIconSize);
      break;
    case HoldingSpaceItem::Type::kScreenshot:
      size = kHoldingSpaceScreenshotSize;
      break;
  }

  // NOTE: We superimpose the file type icon for `file_path` over a transparent
  // bitmap in order to center it within the placeholder image at a fixed size.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width(), size.height());
  return gfx::ImageSkiaOperations::CreateSuperimposedImage(
      gfx::ImageSkia::CreateFrom1xBitmap(bitmap), GetIconForPath(file_path));
}

}  // namespace

ValidityRequirement::ValidityRequirement() = default;
ValidityRequirement::ValidityRequirement(const ValidityRequirement& other) =
    default;
ValidityRequirement::ValidityRequirement(ValidityRequirement&& other) = default;

// Utilities -------------------------------------------------------------------

void FilePathValid(Profile* profile,
                   FilePathWithValidityRequirement file_path_with_requirement,
                   FilePathValidCallback callback) {
  file_manager::util::GetMetadataForPath(
      file_manager::util::GetFileSystemContextForExtensionId(
          profile, file_manager::kFileManagerAppId),
      file_path_with_requirement.first,
      storage::FileSystemOperation::GET_METADATA_FIELD_NONE,
      base::BindOnce(
          [](FilePathValidCallback callback, ValidityRequirement requirement,
             base::File::Error result, const base::File::Info& file_info) {
            bool valid = true;
            if (requirement.must_exist)
              valid = result == base::File::Error::FILE_OK;
            if (valid && requirement.must_be_newer_than) {
              valid = file_info.creation_time >
                      now_for_testing.value_or(base::Time::Now()) -
                          requirement.must_be_newer_than.value();
            }
            std::move(callback).Run(valid);
          },
          std::move(callback), file_path_with_requirement.second));
}

void PartitionFilePathsByValidity(
    Profile* profile,
    FilePathsWithValidityRequirements file_paths_with_requirement,
    PartitionFilePathsByValidityCallback callback) {
  if (file_paths_with_requirement.empty()) {
    std::move(callback).Run(/*valid_file_paths=*/{},
                            /*invalid_file_paths=*/{});
    return;
  }

  auto valid_file_paths = std::make_unique<FilePathList>();
  auto invalid_file_paths = std::make_unique<FilePathList>();

  auto* valid_file_paths_ptr = valid_file_paths.get();
  auto* invalid_file_paths_ptr = invalid_file_paths.get();

  FilePathList file_paths;
  for (const auto& file_path_with_requirement : file_paths_with_requirement)
    file_paths.push_back(file_path_with_requirement.first);

  // This `barrier_closure` will be run after verifying the existence of all
  // `file_paths`. It is expected that both `valid_file_paths` and
  // `invalid_file_paths` will have been populated by the time of
  // invocation.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      file_paths_with_requirement.size(),
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
  for (const auto& file_path_with_requirement : file_paths_with_requirement) {
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

GURL ResolveFileSystemUrl(Profile* profile, const base::FilePath& file_path) {
  GURL file_system_url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, file_path, file_manager::kFileManagerAppId,
          &file_system_url)) {
    VLOG(2) << "Unable to convert file path to File System URL.";
  }
  return file_system_url;
}

std::unique_ptr<HoldingSpaceImage> ResolveImage(
    HoldingSpaceThumbnailLoader* thumbnail_loader,
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return std::make_unique<HoldingSpaceImage>(
      GetPlaceholderImage(type, file_path),
      base::BindRepeating(
          [](const base::WeakPtr<HoldingSpaceThumbnailLoader>& thumbnail_loader,
             const base::FilePath& file_path, const gfx::Size& size,
             HoldingSpaceImage::BitmapCallback callback) {
            if (thumbnail_loader)
              thumbnail_loader->Load({file_path, size}, std::move(callback));
          },
          thumbnail_loader->GetWeakPtr(), file_path));
}

void SetNowForTesting(base::Optional<base::Time> now) {
  now_for_testing = now;
}

}  // namespace holding_space_util
}  // namespace ash
