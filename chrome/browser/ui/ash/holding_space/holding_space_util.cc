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

// Utilities -------------------------------------------------------------------

void FilePathExists(Profile* profile,
                    const base::FilePath& file_path,
                    FilePathExistsCallback callback) {
  if (file_path.empty()) {
    std::move(callback).Run(/*exists=*/false);
    return;
  }
  file_manager::util::GetMetadataForPath(
      file_manager::util::GetFileSystemContextForExtensionId(
          profile, file_manager::kFileManagerAppId),
      file_path, storage::FileSystemOperation::GET_METADATA_FIELD_NONE,
      base::BindOnce(
          [](FilePathExistsCallback callback, base::File::Error result,
             const base::File::Info& file_info) {
            // Absence of error is confirmation of existence.
            bool exists = result == base::File::Error::FILE_OK;
            std::move(callback).Run(exists);
          },
          std::move(callback)));
}

void PartitionFilePathsByExistence(
    Profile* profile,
    FilePathList file_paths,
    PartitionFilePathsByExistenceCallback callback) {
  if (file_paths.empty()) {
    std::move(callback).Run(/*existing_file_paths=*/{},
                            /*non_existing_file_paths=*/{});
    return;
  }

  auto existing_file_paths = std::make_unique<FilePathList>();
  auto non_existing_file_paths = std::make_unique<FilePathList>();

  auto* existing_file_paths_ptr = existing_file_paths.get();
  auto* non_existing_file_paths_ptr = non_existing_file_paths.get();

  // This `barrier_closure` will be run after verifying the existence of all
  // `file_paths`. It is expected that both `existing_file_paths` and
  // `non_existing_file_paths` will have been populated by the time of
  // invocation.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      file_paths.size(),
      base::BindOnce(
          [](FilePathList sorted_file_paths,
             std::unique_ptr<FilePathList> existing_file_paths,
             std::unique_ptr<FilePathList> non_existing_file_paths,
             PartitionFilePathsByExistenceCallback callback) {
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
            sort(existing_file_paths.get());
            sort(non_existing_file_paths.get());

            // Ownership of the partitioned vectors is passed to `callback`.
            std::move(callback).Run(std::move(*existing_file_paths),
                                    std::move(*non_existing_file_paths));
          },
          /*sorted_file_paths=*/file_paths, std::move(existing_file_paths),
          std::move(non_existing_file_paths), std::move(callback)));

  // Verify existence of each `file_path`. Upon successful check of existence,
  // each `file_path` should be pushed into either `existing_file_paths` or
  // `non_existing_file_paths` as appropriate.
  for (auto& file_path : file_paths) {
    FilePathExists(
        profile, file_path,
        base::BindOnce(
            [](base::FilePath file_path, FilePathList* existing_file_paths,
               FilePathList* non_existing_file_paths,
               base::RepeatingClosure barrier_closure, bool exists) {
              if (exists)
                existing_file_paths->push_back(file_path);
              else
                non_existing_file_paths->push_back(file_path);
              barrier_closure.Run();
            },
            file_path, base::Unretained(existing_file_paths_ptr),
            base::Unretained(non_existing_file_paths_ptr), barrier_closure));
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

}  // namespace holding_space_util
}  // namespace ash
