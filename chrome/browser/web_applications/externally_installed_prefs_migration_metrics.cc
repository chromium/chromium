// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_installed_prefs_migration_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace web_app {
void LogPlaceholderMigrationState(
    const PlaceholderMigrationState& migration_state) {
  base::UmaHistogramEnumeration(kPlaceholderMigrationHistogram,
                                migration_state);
}

void LogInstallURLMigrationState(
    const InstallURLMigrationState& migration_state) {
  base::UmaHistogramEnumeration(kInstallURLMigrationHistogram, migration_state);
}

void LogUserUninstalledPreinstalledAppMigration(
    const UserUninstalledPreinstalledAppMigrationState& migration_state) {
  base::UmaHistogramEnumeration(kPreinstalledAppMigrationHistogram,
                                migration_state);
}

}  // namespace web_app
