// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/default_pinned_apps/default_pinned_apps.h"

#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/webui/mall/app_id.h"
#include "build/branding_buildflags.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "chromeos/ash/experiences/arc/app/arc_app_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_context.h"

namespace {

std::vector<StaticAppId> GetDefaultPinnedApps(
    content::BrowserContext* browser_context) {
  std::vector<StaticAppId> app_ids;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (chromeos::features::IsGeminiAppPreinstallEnabled()) {
    app_ids.push_back(ash::kGeminiAppId);
  }
  if (base::FeatureList::IsEnabled(
          chromeos::features::kNotebookLmAppShelfPin)) {
    app_ids.push_back(ash::kNotebookLmAppId);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Pin Mall after AI offerings.
  app_ids.push_back(ash::kMallSystemAppId);

  app_ids.insert(app_ids.end(), {
                                    ash::kGmailAppId,
                                    ash::kGoogleCalendarAppId,
                                    file_manager::kFileManagerSwaAppId,
                                    ash::kMessagesAppId,
                                    ash::kGoogleMeetAppId,
                                    arc::kPlayStoreAppId,
                                    ash::kYoutubeAppId,
                                    arc::kGooglePhotosAppId,
                                });

  if (chromeos::features::IsCloudGamingDeviceEnabled()) {
    app_ids.push_back(ash::kNvidiaGeForceNowAppId);
  }

  return app_ids;
}

std::vector<StaticAppId> GetTabletFormFactorDefaultPinnedApps(
    content::BrowserContext* browser_context) {
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

std::vector<StaticAppId> GetDefaultPinnedAppsForFormFactor(
    content::BrowserContext* browser_context) {
  if (ash::switches::IsTabletFormFactor()) {
    return GetTabletFormFactorDefaultPinnedApps(browser_context);
  }

  return GetDefaultPinnedApps(browser_context);
}
