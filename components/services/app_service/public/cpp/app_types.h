// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_TYPES_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_TYPES_H_

#include <optional>
#include <ostream>

#include "base/component_export.h"
#include "components/services/app_service/public/cpp/macros.h"
#include "components/services/app_service/public/protos/app_types.pb.h"

namespace apps {

// When updating the enum below, update the ApplicationType enum in
// //components/services/app_service/public/protos/app_types.proto.
//
// This is used for metrics and we should not
//   - change the assigned value, nor
//   - reuse the value which was used (even in past historically)
// Email chromeos-data-team@google.com to request a corresponding change to
// backend enums.
enum class AppType {
  kUnknown = 0,
  kArc = 1,  // Android app.
  // kBuiltIn = 2,         Built-in app. (deleted).
  kCrostini = 3,   // Linux (via Crostini) app.
  kChromeApp = 4,  // Chrome app.
  kWeb = 5,        // Web app.
  kPluginVm = 6,   // Plugin VM app, see go/pluginvm.
  // kStandaloneBrowser = 7,  // Removed. No longer used.
  kRemote = 8,      // Remote app.
  kBorealis = 9,    // Borealis app, see go/borealis-app.
  kSystemWeb = 10,  // System web app.
  // kStandaloneBrowserChromeApp = 11,  // Removed. No longer used.
  kExtension = 12,  // Browser extension.
  // kStandaloneBrowserExtension = 13,  // Removed. No longer used.
  kBruschetta = 14,  // Bruschetta app, see go/bruschetta.

  // The value for UMA. Should be updated when a new entry is added.
  kMaxValue = kBruschetta
};

COMPONENT_EXPORT(APP_TYPES)
std::ostream& operator<<(std::ostream& os, AppType v);

// Used by PackageId mapping closely to corresponding values in AppType but
// can contain other non-app values e.g. app shortcuts.
enum class PackageType {
  kUnknown,
  kArc,
  kBorealis,
  kChromeApp,
  kGeForceNow,
  kSystem,
  kWeb,
  // A shortcut to a particular website that's intended to open in a browser,
  // not install as an app.
  kWebsite,
};

COMPONENT_EXPORT(APP_TYPES)
std::ostream& operator<<(std::ostream& os, PackageType v);

// Whether an app is ready to launch, i.e. installed.
//
// This is used for metrics and we should not
//   - change the assigned value, nor
//   - reuse the value which was used (even in past historically)
enum class Readiness {
  kUnknown = 0,
  kReady = 1,                // Installed and launchable.
  kDisabledByBlocklist = 2,  // Disabled by SafeBrowsing.
  kDisabledByPolicy = 3,     // Disabled by admin policy.
  kDisabledByUser = 4,       // Disabled by explicit user action.
  kTerminated = 5,           // Renderer process crashed.
  kUninstalledByUser = 6,
  // Removed apps are purged from the registry cache and have their
  // associated memory freed. Subscribers are not notified of removed
  // apps, so publishers must set the app as uninstalled before
  // removing it.
  kRemoved = 7,
  // This is used for all non-user initiated uninstallation.
  kUninstalledByNonUser = 8,
  kDisabledByLocalSettings = 9,  // Disabled by local settings.

  // The value for UMA. Should be updated when a new entry is added.
  kMaxValue = kDisabledByLocalSettings,
};

COMPONENT_EXPORT(APP_TYPES)
std::ostream& operator<<(std::ostream& os, Readiness v);

// How the app was installed.
// This should be kept in sync with histograms.xml, InstallReason in
// enums.xml as well as ApplicationInstallReason in
// //components/services/app_service/public/protos/app_types.proto.
//
// This is used for metrics and we should not
//   - change the assigned value, nor
//   - reuse the value which was used (even in past historically)
// Email chromeos-data-team@google.com to request a corresponding change to
// backend enums.
enum class InstallReason {
  kUnknown = 0,
  kSystem = 1,  // Installed with the system and is considered a part of the OS.
  kPolicy = 2,  // Installed by policy.
  kOem = 3,     // Installed by an OEM.
  kDefault = 4,  // Preinstalled by default, but is not considered a system app.
  kSync = 5,     // Installed by sync.
  kUser = 6,     // Installed by user action.
  kSubApp = 7,   // Installed by the SubApp API call.
  kKiosk = 8,    // Installed by Kiosk on Chrome OS.
  kCommandLine = 9,  // Deprecated, no longer used.

  // The value for UMA. Should be updated when a new entry is added.
  kMaxValue = kCommandLine,
};

COMPONENT_EXPORT(APP_TYPES)
std::ostream& operator<<(std::ostream& os, InstallReason v);

// Where the app was installed from.
// This should be kept in sync with histograms.xml, InstallSource in
// enums.xml as well as ApplicationInstallSource in
// //components/services/app_service/public/protos/app_types.proto.
//
// This is used for metrics and we should not
//   - change the assigned value, nor
//   - reuse the value which was used (even in past historically)
// Email chromeos-data-team@google.com to request a corresponding change to
// backend enums.
enum class InstallSource {
  kUnknown = 0,
  kSystem = 1,          // Installed as part of Chrome OS.
  kSync = 2,            // Installed from sync.
  kPlayStore = 3,       // Installed from Play store.
  kChromeWebStore = 4,  // Installed from Chrome web store.
  kBrowser = 5,         // Installed from browser.

  // The value for UMA. Should be updated when a new entry is added.
  kMaxValue = kBrowser,
};

COMPONENT_EXPORT(APP_TYPES)
std::ostream& operator<<(std::ostream& os, InstallSource v);

// What caused the app to be uninstalled.
// This should be kept in sync with UninstallSource in enums.xml as well as
// ApplicationUninstallSource in
// //components/services/app_service/public/protos/app_types.proto.
//
// This is used for metrics and we should not
//   - change the assigned value, nor
//   - reuse the value which was used (even in past historically)
// Email chromeos-data-team@google.com to request a corresponding change to
// backend enums.
enum class UninstallSource {
  kUnknown = 0,
  kAppList = 1,        // Uninstall by the user from the App List (Launcher)
  kAppManagement = 2,  // Uninstall by the user from the App Management page
  kShelf = 3,          // Uninstall by the user from the Shelf
  kMigration = 4,      // Uninstall by app migration.

  // The value for UMA. Should be updated when a new entry is added.
  kMaxValue = kMigration,
};

// The window mode that each app will open in.
//
// This is used for metrics and we should not
//   - change the assigned value, nor
//   - reuse the value which was used (even in past historically)
enum class WindowMode {
  kUnknown,
  // Opens in a standalone window
  kWindow,
  // Opens in the default web browser
  kBrowser,
  // Opens in a tabbed app window
  kTabbedWindow,

  // The value for GetEnumFromKey. Should be updated when a new entry is added.
  kMaxValue = kTabbedWindow,
};

COMPONENT_EXPORT(APP_TYPES)
std::ostream& operator<<(std::ostream& os, WindowMode v);

COMPONENT_EXPORT(APP_TYPES)
ApplicationType ConvertAppTypeToProtoApplicationType(AppType app_type);

COMPONENT_EXPORT(APP_TYPES)
std::optional<AppType> ConvertPackageTypeToAppType(PackageType package_type);

COMPONENT_EXPORT(APP_TYPES)
std::optional<PackageType> ConvertAppTypeToPackageType(AppType app_type);

COMPONENT_EXPORT(APP_TYPES)
ApplicationInstallReason ConvertInstallReasonToProtoApplicationInstallReason(
    InstallReason install_reason);

COMPONENT_EXPORT(APP_TYPES)
ApplicationInstallSource ConvertInstallSourceToProtoApplicationInstallSource(
    InstallSource install_source);

COMPONENT_EXPORT(APP_TYPES)
ApplicationUninstallSource
ConvertUninstallSourceToProtoApplicationUninstallSource(
    UninstallSource uninstall_source);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_TYPES_H_
