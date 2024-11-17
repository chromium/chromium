// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/default_pinned_apps/default_pinned_apps.h"

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_factory.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_context.h"

namespace {

bool ShouldAddHelpApp(content::BrowserContext* browser_context) {
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser_context);
  return scalable_iph && scalable_iph->ShouldPinHelpAppToShelf();
}

std::vector<StaticAppId> GetDefaultPinnedApps(
    content::BrowserContext* browser_context) {
  std::vector<StaticAppId> app_ids{
      ash::kGmailAppId,

      ash::kGoogleCalendarAppId,

      file_manager::kFileManagerSwaAppId,

      ash::kMessagesAppId,

      ash::kGoogleMeetAppId,

      arc::kPlayStoreAppId,

      ash::kYoutubeAppId,

      arc::kGooglePhotosAppId,
  };

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (chromeos::features::IsGeminiAppPreinstallEnabled()) {
    app_ids.insert(app_ids.begin(), ash::kGeminiAppId);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  if (chromeos::features::IsCloudGamingDeviceEnabled()) {
    app_ids.push_back(ash::kNvidiaGeForceNowAppId);
  }

  if (ShouldAddHelpApp(browser_context)) {
    app_ids.push_back(ash::kHelpAppId);
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

  if (ShouldAddHelpApp(browser_context)) {
    app_ids.push_back(ash::kHelpAppId);
  }

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
