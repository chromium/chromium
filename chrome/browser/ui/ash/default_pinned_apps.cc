// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/default_pinned_apps.h"

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"

namespace {

base::span<StaticAppId> GetDefaultPinnedApps() {
  constexpr const char* kDefaultPinnedApps[] = {
      extension_misc::kGmailAppId,
      web_app::kGmailAppId,

      web_app::kGoogleCalendarAppId,

      // TODO(b/207576430): Once Files SWA is fully launched, remove this entry.
      extension_misc::kFilesManagerAppId,

      file_manager::kFileManagerSwaAppId,

      web_app::kMessagesAppId,

      web_app::kGoogleMeetAppId,

      arc::kPlayStoreAppId,

      extension_misc::kYoutubeAppId,
      web_app::kYoutubeAppId,

      arc::kGooglePhotosAppId,
  };
  return base::span<StaticAppId>(kDefaultPinnedApps,
                                 std::size(kDefaultPinnedApps));
}

base::span<StaticAppId> GetTabletFormFactorDefaultPinnedApps() {
  constexpr const char* kTabletFormFactorDefaultPinnedApps[] = {
      arc::kGmailAppId,

      arc::kGoogleCalendarAppId,

      arc::kPlayStoreAppId,

      arc::kYoutubeAppId,

      arc::kGooglePhotosAppId,
  };
  return base::span<StaticAppId>(kTabletFormFactorDefaultPinnedApps,
                                 std::size(kTabletFormFactorDefaultPinnedApps));
}

}  // namespace

base::span<StaticAppId> GetDefaultPinnedAppsForFormFactor() {
  if (ash::switches::IsTabletFormFactor()) {
    return GetTabletFormFactorDefaultPinnedApps();
  }

  return GetDefaultPinnedApps();
}
