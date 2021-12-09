// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_util.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#endif

bool IsAppLauncherEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return false;
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace app_list {
std::unique_ptr<ash::AppListItemMetadata> GenerateItemMetadataFromSyncItem(
    const app_list::AppListSyncableService::SyncItem& sync_item) {
  auto item_meta_data = std::make_unique<ash::AppListItemMetadata>();
  item_meta_data->id = sync_item.item_id;
  item_meta_data->position = sync_item.item_ordinal;
  item_meta_data->is_folder =
      (sync_item.item_type == sync_pb::AppListSpecifics::TYPE_FOLDER);
  item_meta_data->is_page_break =
      (sync_item.item_type == sync_pb::AppListSpecifics::TYPE_PAGE_BREAK);
  item_meta_data->name = sync_item.item_name;
  item_meta_data->folder_id = sync_item.parent_id;
  if (ash::features::IsLauncherItemColorSyncEnabled())
    item_meta_data->icon_color = sync_item.item_color;

  return item_meta_data;
}
}  // namespace app_list
#endif
