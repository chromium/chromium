// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_

#include <memory>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/callback_forward.h"

class GURL;
class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace ash {

class HoldingSpaceImage;
class HoldingSpaceThumbnailLoader;

// A utility for holding space.
namespace holding_space_util {

using FilePathList = std::vector<base::FilePath>;

// Checks `file_path` existence, returning the result via `callback`.
using FilePathExistsCallback = base::OnceCallback<void(bool)>;
void FilePathExists(Profile* profile,
                    const base::FilePath& file_path,
                    FilePathExistsCallback callback);

// Partitions `file_paths` into `existing_file_paths` and
// `non_existing_file_paths`, returning the result via `callback`.
using PartitionFilePathsByExistenceCallback =
    base::OnceCallback<void(FilePathList existing_file_paths,
                            FilePathList non_existing_file_paths)>;
void PartitionFilePathsByExistence(
    Profile* profile,
    FilePathList file_paths,
    PartitionFilePathsByExistenceCallback callback);

// Resolves the file system URL associated with the specified `file_path`.
GURL ResolveFileSystemUrl(Profile* profile, const base::FilePath& file_path);

// Resolves the image associated with the specified `file_path`.
std::unique_ptr<HoldingSpaceImage> ResolveImage(
    HoldingSpaceThumbnailLoader* thumbnail_loader,
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path);

}  // namespace holding_space_util
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
