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
// This is used for metrics and should not be reordered or removed and email
// chromeos-data-team@google.com to request a corresponding change to backend
// enums.
enum class AppType {
  kUnknown = 0,
  kArc = 1,                // Android app.
  kBuiltIn = 2,            // Built-in app. (No longer used)
  kCrostini = 3,           // Linux (via Crostini) app.
  kChromeApp = 4,          // Chrome app.
  kWeb = 5,                // Web app.
  kPluginVm = 6,           // Plugin VM app, see go/pluginvm.
  kStandaloneBrowser = 7,  // Lacros browser app, see //docs/lacros.md.
  kRemote = 8,             // Remote app.
  kBorealis = 9,           // Borealis app, see go/borealis-app.
  kSystemWeb = 10,         // System web app.
  kStandaloneBrowserChromeApp = 11,  // Chrome app hosted in Lacros.
  kExtension = 12,                   // Browser extension.
  kStandaloneBrowserExtension = 13,  // Extension hosted in Lacros.
  kBruschetta = 14,                  // Bruschetta app, see go/bruschetta.

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
// Note the enumeration is used in UMA histogram so entries should not be
// re-ordered or removed. New entries should be added at the bottom.
ENUM(Readiness,
     kUnknown,
     kReady,                // Installed and launchable.
     kDisabledByBlocklist,  // Disabled by SafeBrowsing.
     kDisabledByPolicy,     // Disabled by admin policy.
     kDisabledByUser,       // Disabled by explicit user action.
     kTerminated,           // Renderer process crashed.
     kUninstalledByUser,
     // Removed apps are purged from the registry cache and have their
     // associated memory freed. Subscribers are not notified of removed
     // apps, so publishers must set the app as uninstalled before
     // removing it.
     kRemoved,
     // This is used for all non-user initiated uninstallation.
     kUninstalledByNonUser,
     kDisabledByLocalSettings  // Disabled by local settings.
)

// How the app was installed.
// This should be kept in sync with histograms.xml, InstallReason in
// enums.xml as well as ApplicationInstallReason in
// //components/services/app_service/public/protos/app_types.proto.
//
// Email chromeos-data-team@google.com to request a corresponding change to
// backend enums.
//
// Note the enumeration is used in UMA histogram so entries should not be
// re-ordered or removed. New entries should be added at the bottom.
ENUM(InstallReason,
     kUnknown,
     kSystem,   // Installed with the system and is considered a part of the OS.
     kPolicy,   // Installed by policy.
     kOem,      // Installed by an OEM.
     kDefault,  // Preinstalled by default, but is not considered a system app.
     kSync,     // Installed by sync.
     kUser,     // Installed by user action.
     kSubApp,   // Installed by the SubApp API call.
     kKiosk,    // Installed by Kiosk on Chrome OS.
     kCommandLine  // Deprecated, no longer used.
)

// Where the app was installed from.
// This should be kept in sync with histograms.xml, InstallSource in
// enums.xml as well as ApplicationInstallSource in
// //components/services/app_service/public/protos/app_types.proto.
//
// Email chromeos-data-team@google.com to request a corresponding change to
// backend enums.
//
// Note the enumeration is used in UMA histogram so entries should not be
// re-ordered or removed. New entries should be added at the bottom.
ENUM(InstallSource,
     kUnknown,
     kSystem,          // Installed as part of Chrome OS.
     kSync,            // Installed from sync.
     kPlayStore,       // Installed from Play store.
     kChromeWebStore,  // Installed from Chrome web store.
     kBrowser          // Installed from browser.
)

// What caused the app to be uninstalled.
// This should be kept in sync with UninstallSource in enums.xml as well as
// ApplicationUninstallSource in
// //components/services/app_service/public/protos/app_types.proto, so entries
// should not be re-ordered or removed. New entries should be added at the
// bottom.
//
// Email chromeos-data-team@google.com to request a corresponding change to
// backend enums.
ENUM(UninstallSource,
     kUnknown,
     kAppList,        // Uninstall by the user from the App List (Launcher)
     kAppManagement,  // Uninstall by the user from the App Management page
     kShelf,          // Uninstall by the user from the Shelf
     kMigration       // Uninstall by app migration.
)

// The window mode that each app will open in.
ENUM(WindowMode,
     kUnknown,
     // Opens in a standalone window
     kWindow,
     // Opens in the default web browser
     kBrowser,
     // Opens in a tabbed app window
     kTabbedWindow)

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
