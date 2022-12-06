// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/default_pinned_apps.h"

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "extensions/common/constants.h"

namespace {

std::vector<StaticAppId> GetDefaultPinnedApps() {
  std::vector<StaticAppId> app_ids{
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

  if (chromeos::features::IsCloudGamingDeviceEnabled()) {
    app_ids.push_back(web_app::kCloudGamingPartnerPlatform);
    app_ids.push_back(web_app::kStadiaAppId);
  }

  return app_ids;
}

std::vector<StaticAppId> GetTabletFormFactorDefaultPinnedApps() {
  std::vector<StaticAppId> app_ids{
      arc::kGmailAppId,

      arc::kGoogleCalendarAppId,

      arc::kPlayStoreAppId,

      arc::kYoutubeAppId,

      arc::kGooglePhotosAppId,
  };
  return app_ids;
}

}  // namespace

std::vector<StaticAppId> GetDefaultPinnedAppsForFormFactor() {
  if (ash::switches::IsTabletFormFactor()) {
    return GetTabletFormFactorDefaultPinnedApps();
  }

  return GetDefaultPinnedApps();
}
