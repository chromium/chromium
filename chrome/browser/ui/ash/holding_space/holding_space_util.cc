// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"

#include <map>

#include "ash/public/cpp/file_icon_util.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "url/gurl.h"

namespace ash {
namespace holding_space_util {

void ItemExists(Profile* profile,
                const HoldingSpaceItem* item,
                ItemExistsCallback callback) {
  if (!item) {
    std::move(callback).Run(/*exists=*/false);
    return;
  }
  file_manager::util::GetMetadataForPath(
      file_manager::util::GetFileSystemContextForExtensionId(
          profile, file_manager::kFileManagerAppId),
      item->file_path(), storage::FileSystemOperation::GET_METADATA_FIELD_NONE,
      base::BindOnce(
          [](ItemExistsCallback callback, base::File::Error result,
             const base::File::Info& file_info) {
            // Absence of error is confirmation of existence.
            bool exists = result == base::File::Error::FILE_OK;
            std::move(callback).Run(exists);
          },
          std::move(callback)));
}

void PartitionItemsByExistence(Profile* profile,
                               HoldingSpaceItemPtrList items,
                               PartitionItemsByExistenceCallback callback) {
  if (items.empty()) {
    std::move(callback).Run(/*existing_items=*/{}, /*non_existing_items=*/{});
    return;
  }

  // Cache the original indices of the `items` being partitioned so that we can
  // return them back in the same order after checking for existence.
  std::map<std::string, size_t> indices_by_id;
  for (size_t i = 0; i < items.size(); ++i)
    indices_by_id[items.at(i)->id()] = i;

  auto existing_items = std::make_unique<HoldingSpaceItemPtrList>();
  auto non_existing_items = std::make_unique<HoldingSpaceItemPtrList>();

  auto* existing_items_ptr = existing_items.get();
  auto* non_existing_items_ptr = non_existing_items.get();

  // This `barrier_closure` will be run after verifying the existence of all
  // holding space `items`. It is expected that both `existing_items` and
  // `non_existing_items` will have been populated by the time of invocation.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      items.size(),
      base::BindOnce(
          [](std::unique_ptr<HoldingSpaceItemPtrList> existing_items,
             std::unique_ptr<HoldingSpaceItemPtrList> non_existing_items,
             std::map<std::string, size_t> indices_by_id,
             PartitionItemsByExistenceCallback callback) {
            // We need to sort our partitioned vectors to match the original
            // order that was provided at call time. This is necessary as the
            // original order may have been lost due to race conditions when
            // checking for item existence.
            auto sort = [&indices_by_id](HoldingSpaceItemPtrList* items) {
              std::sort(items->begin(), items->end(),
                        [&indices_by_id](const auto& a, const auto& b) {
                          return indices_by_id[a->id()] <
                                 indices_by_id[b->id()];
                        });
            };
            sort(existing_items.get());
            sort(non_existing_items.get());

            // Ownership of the partitioned vectors is passed to `callback`.
            std::move(callback).Run(std::move(*existing_items),
                                    std::move(*non_existing_items));
          },
          std::move(existing_items), std::move(non_existing_items),
          std::move(indices_by_id), std::move(callback)));

  // Verify existence of each holding space `item`. Upon successful check of
  // existence, each `item` should be pushed into either `existing_items` or
  // `non_existing_items` as appropriate.
  for (auto& item : items) {
    HoldingSpaceItem* item_ptr = item.get();
    ItemExists(profile, item_ptr,
               base::BindOnce(
                   [](HoldingSpaceItemPtr item,
                      HoldingSpaceItemPtrList* existing_items,
                      HoldingSpaceItemPtrList* non_existing_items,
                      base::RepeatingClosure barrier_closure, bool exists) {
                     if (exists)
                       existing_items->push_back(std::move(item));
                     else
                       non_existing_items->push_back(std::move(item));
                     barrier_closure.Run();
                   },
                   std::move(item), base::Unretained(existing_items_ptr),
                   base::Unretained(non_existing_items_ptr), barrier_closure));
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

// TODO(dmblack): Use thumbnail service to asynchronously replace placeholders.
std::unique_ptr<HoldingSpaceImage> ResolveImage(
    const base::FilePath& file_path) {
  return std::make_unique<HoldingSpaceImage>(
      /*placeholder=*/GetIconForPath(file_path));
}

}  // namespace holding_space_util
}  // namespace ash
