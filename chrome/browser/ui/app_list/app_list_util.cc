// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_util.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

bool IsAppLauncherEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return false;
#endif
}

namespace app_list {
#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<ash::AppListItemMetadata> GenerateItemMetadataFromSyncItem(
    const app_list::AppListSyncableService::SyncItem& sync_item) {
  DCHECK(sync_item.item_type != sync_pb::AppListSpecifics::TYPE_PAGE_BREAK);

  auto item_meta_data = std::make_unique<ash::AppListItemMetadata>();
  item_meta_data->id = sync_item.item_id;
  item_meta_data->position = sync_item.item_ordinal;
  item_meta_data->is_folder =
      (sync_item.item_type == sync_pb::AppListSpecifics::TYPE_FOLDER);
  item_meta_data->name = sync_item.item_name;
  item_meta_data->folder_id = sync_item.parent_id;
  if (ash::features::IsLauncherItemColorSyncEnabled())
    item_meta_data->icon_color = sync_item.item_color;

  return item_meta_data;
}
#endif

bool IsResultTypeApp(ash::AppListSearchResultType result_type) {
  switch (result_type) {
    case ash::AppListSearchResultType::kInstalledApp:
    case ash::AppListSearchResultType::kZeroStateApp:
    case ash::AppListSearchResultType::kInternalApp:
    case ash::AppListSearchResultType::kGames:
      return true;
    case ash::AppListSearchResultType::kOmnibox:
    case ash::AppListSearchResultType::kLauncher:
    case ash::AppListSearchResultType::kFileSearch:
    case ash::AppListSearchResultType::kDriveSearch:
    case ash::AppListSearchResultType::kUnknown:
    case ash::AppListSearchResultType::kPlayStoreApp:
    case ash::AppListSearchResultType::kInstantApp:
    case ash::AppListSearchResultType::kAnswerCard:
    case ash::AppListSearchResultType::kPlayStoreReinstallApp:
    case ash::AppListSearchResultType::kOsSettings:
    case ash::AppListSearchResultType::kInternalPrivacyInfo:
    case ash::AppListSearchResultType::kAssistantText:
    case ash::AppListSearchResultType::kPersonalization:
    case ash::AppListSearchResultType::kHelpApp:
    case ash::AppListSearchResultType::kZeroStateHelpApp:
    case ash::AppListSearchResultType::kKeyboardShortcut:
    case ash::AppListSearchResultType::kOpenTab:
    case ash::AppListSearchResultType::kArcAppShortcut:
    case ash::AppListSearchResultType::kZeroStateFile:
    case ash::AppListSearchResultType::kZeroStateDrive:
      return false;
  }
}

}  // namespace app_list
