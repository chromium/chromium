// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"

class GURL;
class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace ash {

class HoldingSpaceImage;
class HoldingSpaceItem;

using HoldingSpaceItemPtr = std::unique_ptr<HoldingSpaceItem>;
using HoldingSpaceItemPtrList = std::vector<HoldingSpaceItemPtr>;

// A utility for holding space.
namespace holding_space_util {

// Checks `item` existence, returning the result via `callback`.
using ItemExistsCallback = base::OnceCallback<void(bool)>;
void ItemExists(Profile* profile,
                const HoldingSpaceItem* item,
                ItemExistsCallback callback);

// Partitions `items` into `existing_items` and `non_existing_items`, returning
// the result via `callback`.
using PartitionItemsByExistenceCallback =
    base::OnceCallback<void(HoldingSpaceItemPtrList existing_items,
                            HoldingSpaceItemPtrList non_existing_items)>;
void PartitionItemsByExistence(Profile* profile,
                               HoldingSpaceItemPtrList items,
                               PartitionItemsByExistenceCallback callback);

// Resolves the file system URL associated with the specified `file_path`.
GURL ResolveFileSystemUrl(Profile* profile, const base::FilePath& file_path);

// Resolves the image associated with the specified `file_path`.
std::unique_ptr<HoldingSpaceImage> ResolveImage(
    const base::FilePath& file_path);

}  // namespace holding_space_util
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
