// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_INSTALLED_PREFS_MIGRATION_METRICS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_INSTALLED_PREFS_MIGRATION_METRICS_H_

namespace web_app {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PlaceholderMigrationState {
  // Migration to web_app DB successful for placeholder info.
  kPlaceholderInfoMigrated = 0,

  // Migration to web_app DB skipped because data is in sync, or has been
  // already migrated.
  kPlaceholderInfoAlreadyInSync = 1,

  kMaxValue = kPlaceholderInfoAlreadyInSync
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class InstallURLMigrationState {
  // Migration to web_app DB successful for install_url.
  kInstallURLMigrated = 0,

  // Migration to web_app DB skipped because data is in sync, or has been
  // already migrated.
  kInstallURLAlreadyInSync = 1,

  kMaxValue = kInstallURLAlreadyInSync
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UserUninstalledPreinstalledAppMigrationState {
  // Migration to UserUninstalledPreinstalledWebAppPrefs successful due to user
  // uninstalls.
  kPreinstalledAppDataMigratedByUser = 0,

  // Migration to UserUninstalledPreinstalledWebAppPrefs successful due to true
  // value in kWasExternalAppUninstalledByUser.
  kPreinstalledAppDataMigratedByOldPref = 1,

  // Migration to UserUninstalledPreinstalledWebAppPrefs skipped because data is
  // in sync, or has been already migrated.
  kPreinstalledAppDataAlreadyInSync = 2,

  kMaxValue = kPreinstalledAppDataAlreadyInSync
};

const char kPrefDataAbsentDBDataAbsent[] =
    "WebApp.ExternalPrefs.PrefDataAbsentDBDataAbsent";
const char kPrefDataAbsentDBDataPresent[] =
    "WebApp.ExternalPrefs.PrefDataAbsentDBDataPresent";
const char kPrefDataPresentDBDataAbsent[] =
    "WebApp.ExternalPrefs.PrefDataPresentDBDataAbsent";
const char kPrefDataPresentDBDataPresent[] =
    "WebApp.ExternalPrefs.PrefDataPresentDBDataPresent";
const char kPlaceholderMigrationHistogram[] =
    "WebApp.ExternalPrefs.PlaceholderMigrationState";
const char kInstallURLMigrationHistogram[] =
    "WebApp.ExternalPrefs.InstallURLMigrationState";
const char kPreinstalledAppMigrationHistogram[] =
    "WebApp.ExternalPrefs.UserUninstalledPreinstalledAppMigrationState";

// Setting functions
void LogPlaceholderMigrationState(
    const PlaceholderMigrationState& migration_state);
void LogInstallURLMigrationState(
    const InstallURLMigrationState& migration_state);
void LogUserUninstalledPreinstalledAppMigration(
    const UserUninstalledPreinstalledAppMigrationState& migration_state);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_INSTALLED_PREFS_MIGRATION_METRICS_H_
