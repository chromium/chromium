// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/ash/migrations/migrate_preinstalls_to_aps.h"

#include <string_view>
#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace web_app::migrations {

void MigratePreinstallsToAps(WebAppSyncBridge* sync_bridge) {
  std::vector<std::string> migrate_default;
  std::vector<std::string> migrate_oem;
  int count = 0;
  for (const web_app::WebApp& web_app : sync_bridge->registrar().GetApps()) {
    ++count;
    // Ignore core 6 apps: gmail, docs, drive, sheets, slides, yt.
    static constexpr auto kCoreApps = base::MakeFixedFlatSet<std::string_view>(
        {kGmailAppId, kGoogleDocsAppId, kGoogleDriveAppId, kGoogleSheetsAppId,
         kGoogleSlidesAppId, kYoutubeAppId});
    if (kCoreApps.contains(web_app.app_id())) {
      continue;
    }
    if (web_app.GetSources().Has(WebAppManagement::Type::kDefault)) {
      if (web_app.chromeos_data() && web_app.chromeos_data()->oem_installed) {
        migrate_oem.push_back(web_app.app_id());
      } else {
        migrate_default.push_back(web_app.app_id());
      }
    }
  }
  web_app::ScopedRegistryUpdate update = sync_bridge->BeginUpdate();

  VLOG(1) << "Migrate apps=" << count << ", default=" << migrate_default.size()
          << ", oem=" << migrate_oem.size();
  // Add source kApsDefault or kOem.  The kDefault source will be removed when
  // GetChromeBrandedApps() stops including these non-core apps.
  for (const std::string& app_id : migrate_default) {
    web_app::WebApp* app = update->UpdateApp(app_id);
    if (!app) {
      LOG(ERROR) << "Default app not found: " << app_id;
      continue;
    }
    VLOG(1) << "Default: " << app_id;
    app->AddSource(WebAppManagement::Type::kApsDefault);
  }
  for (const std::string& app_id : migrate_oem) {
    web_app::WebApp* app = update->UpdateApp(app_id);
    if (!app) {
      LOG(ERROR) << "OEM app not found: " << app_id;
      continue;
    }
    VLOG(1) << "OEM: " << app_id;
    app->AddSource(WebAppManagement::Type::kOem);
  }
}

}  // namespace web_app::migrations
