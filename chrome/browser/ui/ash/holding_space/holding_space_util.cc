// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/barrier_closure.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "storage/browser/file_system/file_system_context.h"

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
             PartitionItemsByExistenceCallback callback) {
            std::move(callback).Run(std::move(*existing_items),
                                    std::move(*non_existing_items));
          },
          std::move(existing_items), std::move(non_existing_items),
          std::move(callback)));

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

}  // namespace holding_space_util
}  // namespace ash
