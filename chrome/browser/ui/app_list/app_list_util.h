// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_UTIL_H_

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"

namespace ash {
struct AppListItemMetadata;
}
#endif

// Returns whether the app launcher has been enabled.
bool IsAppLauncherEnabled();

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace app_list {

// Generates app list item meta data from the given sync item. Note that
// `AppListItemMetadata` has more attributes than `SyncItem`. Those extra
// attributes are not covered by this method.
std::unique_ptr<ash::AppListItemMetadata> GenerateItemMetadataFromSyncItem(
    const app_list::AppListSyncableService::SyncItem& sync_item);

}  // namespace app_list
#endif

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_UTIL_H_
