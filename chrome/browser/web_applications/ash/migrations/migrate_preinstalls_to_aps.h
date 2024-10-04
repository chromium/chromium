// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ASH_MIGRATIONS_MIGRATE_PREINSTALLS_TO_APS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ASH_MIGRATIONS_MIGRATE_PREINSTALLS_TO_APS_H_

namespace web_app {

class WebAppSyncBridge;

namespace migrations {

// This migration is part of switching the management of preinstalled apps from
// PreinstalledWebAppManager (PWAM) to AppPreloadService (APS). Core apps
// (gmail, docs, drive, sheets, slides, yt) will continue to sync via PWAM, but
// all other apps will be controlled by APS.
//
// This migration adds `kApsDefault` source to non-core preinstalled apps set
// with `kDefault`, and add `kOem` to apps set with `oem_installed` so that the
// apps will not be deleted when they stop syncing via PWAM.
void MigratePreinstallsToAps(WebAppSyncBridge* sync_bridge);

}  // namespace migrations

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ASH_MIGRATIONS_MIGRATE_PREINSTALLS_TO_APS_H_
