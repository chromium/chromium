// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

class GURL;
class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace ash {

class HoldingSpaceImage;
class ThumbnailLoader;

// A utility for holding space.
namespace holding_space_util {

struct ValidityRequirement {
  ValidityRequirement();
  ValidityRequirement(const ValidityRequirement& other);
  ValidityRequirement(ValidityRequirement&& other);
  bool must_exist = true;
  std::optional<base::TimeDelta> must_be_newer_than = std::nullopt;
};

using FilePathList = std::vector<base::FilePath>;
using FilePathWithValidityRequirement =
    std::pair<base::FilePath, ValidityRequirement>;
using FilePathsWithValidityRequirements =
    std::vector<FilePathWithValidityRequirement>;

// Checks `file_path` validity, returning the result via `callback`.
using FilePathValidCallback = base::OnceCallback<void(bool)>;
void FilePathValid(Profile*,
                   FilePathWithValidityRequirement,
                   FilePathValidCallback);

// Partitions `file_paths` into `existing_file_paths` and
// `non_existing_file_paths`, returning the result via `callback`.
using PartitionFilePathsByExistenceCallback =
    base::OnceCallback<void(FilePathList existing_file_paths,
                            FilePathList invalid_file_paths)>;
void PartitionFilePathsByExistence(Profile*,
                                   FilePathList,
                                   PartitionFilePathsByExistenceCallback);

// Partitions `file_paths` into `valid_file_paths` and
// `invalid_file_paths`, returning the result via `callback`.
using PartitionFilePathsByValidityCallback =
    base::OnceCallback<void(FilePathList valid_file_paths,
                            FilePathList invalid_file_paths)>;
void PartitionFilePathsByValidity(Profile*,
                                  FilePathsWithValidityRequirements,
                                  PartitionFilePathsByValidityCallback);

// Resolves the file system type associated with the specified
// `file_system_url`.
HoldingSpaceFile::FileSystemType ResolveFileSystemType(
    Profile* profile,
    const GURL& file_system_url);

// Resolves the file system URL associated with the specified `file_path`.
GURL ResolveFileSystemUrl(Profile* profile, const base::FilePath& file_path);

// Resolves the image associated with the specified `file_path` using the
// default placeholder resolver which creates a placeholder corresponding to the
// associated file type when a thumbnail cannot be generated.
std::unique_ptr<HoldingSpaceImage> ResolveImage(
    ThumbnailLoader* thumbnail_loader,
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path);

// Resolves the image associated with the specified `file_path`using the
// specified `placeholder_image_skia_resolver` to create a placeholder when a
// thumbnail cannot be generated.
std::unique_ptr<HoldingSpaceImage> ResolveImageWithPlaceholderImageSkiaResolver(
    ThumbnailLoader* thumbnail_loader,
    HoldingSpaceImage::PlaceholderImageSkiaResolver
        placeholder_image_skia_resolver,
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path);

}  // namespace holding_space_util
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
