// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/default_pinned_apps.h"

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/scalable_iph/scalable_iph_factory.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/constants.h"

namespace {

bool ShouldAddHelpApp(content::BrowserContext* browser_context) {
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser_context);
  return scalable_iph && scalable_iph->ShouldPinHelpAppToShelf();
}

std::vector<StaticAppId> GetDefaultPinnedApps(
    content::BrowserContext* browser_context) {
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
    app_ids.push_back(web_app::kNvidiaGeForceNowAppId);
  }

  if (ShouldAddHelpApp(browser_context)) {
    app_ids.push_back(web_app::kHelpAppId);
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
    app_ids.push_back(web_app::kHelpAppId);
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
