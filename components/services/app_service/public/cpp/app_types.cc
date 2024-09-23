// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

APP_ENUM_TO_STRING(AppType,
                   kUnknown,
                   kArc,
                   kBuiltIn,
                   kCrostini,
                   kChromeApp,
                   kWeb,
                   kPluginVm,
                   kStandaloneBrowser,
                   kRemote,
                   kBorealis,
                   kSystemWeb,
                   kStandaloneBrowserChromeApp,
                   kExtension,
                   kStandaloneBrowserExtension,
                   kBruschetta)
APP_ENUM_TO_STRING(PackageType,
                   kUnknown,
                   kArc,
                   kBorealis,
                   kChromeApp,
                   kGeForceNow,
                   kSystem,
                   kWeb,
                   kWebsite)
APP_ENUM_TO_STRING(Readiness,
                   kUnknown,
                   kReady,
                   kDisabledByBlocklist,
                   kDisabledByPolicy,
                   kDisabledByUser,
                   kTerminated,
                   kUninstalledByUser,
                   kRemoved,
                   kUninstalledByNonUser,
                   kDisabledByLocalSettings)
APP_ENUM_TO_STRING(InstallReason,
                   kUnknown,
                   kSystem,
                   kPolicy,
                   kOem,
                   kDefault,
                   kSync,
                   kUser,
                   kSubApp,
                   kKiosk,
                   kCommandLine)
APP_ENUM_TO_STRING(InstallSource,
                   kUnknown,
                   kSystem,
                   kSync,
                   kPlayStore,
                   kChromeWebStore,
                   kBrowser)
APP_ENUM_TO_STRING(WindowMode, kUnknown, kWindow, kBrowser, kTabbedWindow)

ApplicationType ConvertAppTypeToProtoApplicationType(AppType app_type) {
  switch (app_type) {
    case AppType::kUnknown:
      return ApplicationType::APPLICATION_TYPE_UNKNOWN;
    case AppType::kArc:
      return ApplicationType::APPLICATION_TYPE_ARC;
    case AppType::kBuiltIn:
      return ApplicationType::APPLICATION_TYPE_BUILT_IN;
    case AppType::kCrostini:
      return ApplicationType::APPLICATION_TYPE_CROSTINI;
    case AppType::kChromeApp:
      return ApplicationType::APPLICATION_TYPE_CHROME_APP;
    case AppType::kWeb:
      return ApplicationType::APPLICATION_TYPE_WEB;
    case AppType::kPluginVm:
      return ApplicationType::APPLICATION_TYPE_PLUGIN_VM;
    case AppType::kStandaloneBrowser:
      return ApplicationType::APPLICATION_TYPE_STANDALONE_BROWSER;
    case AppType::kRemote:
      return ApplicationType::APPLICATION_TYPE_REMOTE;
    case AppType::kBorealis:
      return ApplicationType::APPLICATION_TYPE_BOREALIS;
    case AppType::kSystemWeb:
      return ApplicationType::APPLICATION_TYPE_SYSTEM_WEB;
    case AppType::kStandaloneBrowserChromeApp:
      return ApplicationType::APPLICATION_TYPE_STANDALONE_BROWSER_CHROME_APP;
    case AppType::kExtension:
      return ApplicationType::APPLICATION_TYPE_EXTENSION;
    case AppType::kStandaloneBrowserExtension:
      return ApplicationType::APPLICATION_TYPE_STANDALONE_BROWSER_EXTENSION;
    case AppType::kBruschetta:
      return ApplicationType::APPLICATION_TYPE_BRUSCHETTA;
  }
}

std::optional<AppType> ConvertPackageTypeToAppType(PackageType package_type) {
  switch (package_type) {
    case PackageType::kUnknown:
      return AppType::kUnknown;
    case PackageType::kArc:
      return AppType::kArc;
    case PackageType::kBorealis:
      return AppType::kBorealis;
    case PackageType::kChromeApp:
      return AppType::kChromeApp;
    case PackageType::kGeForceNow:
      return std::nullopt;
    case PackageType::kSystem:
      return std::nullopt;
    case PackageType::kWeb:
      return AppType::kWeb;
    case PackageType::kWebsite:
      return std::nullopt;
  }
}

std::optional<PackageType> ConvertAppTypeToPackageType(AppType app_type) {
  switch (app_type) {
    case AppType::kUnknown:
      return PackageType::kUnknown;
    case AppType::kArc:
      return PackageType::kArc;
    case AppType::kChromeApp:
      return PackageType::kChromeApp;
    case AppType::kWeb:
      return PackageType::kWeb;
    case AppType::kBorealis:
      return PackageType::kBorealis;
    case AppType::kBruschetta:
    case AppType::kBuiltIn:
    case AppType::kCrostini:
    case AppType::kPluginVm:
    case AppType::kStandaloneBrowser:
    case AppType::kRemote:
    case AppType::kSystemWeb:
    case AppType::kStandaloneBrowserChromeApp:
    case AppType::kExtension:
    case AppType::kStandaloneBrowserExtension:
      return std::nullopt;
  }
}

ApplicationInstallReason ConvertInstallReasonToProtoApplicationInstallReason(
    InstallReason install_reason) {
  switch (install_reason) {
    case InstallReason::kUnknown:
      return ApplicationInstallReason::APPLICATION_INSTALL_REASON_UNKNOWN;
    case InstallReason::kSystem:
      return ApplicationInstallReason::APPLICATION_INSTALL_REASON_SYSTEM;
    case InstallReason::kPolicy:
      return ApplicationInstallReason::APPLICATION_INSTALL_REASON_POLICY;
    case InstallReason::kOem:
      return ApplicationInstallReason::APPLICATION_INSTALL_REASON_OEM;
    case InstallReason::kDefault:
      return ApplicationInstallReason::APPLICATION_INSTALL_REASON_DEFAULT;
    case InstallReason::kSync:
      return ApplicationInstallReason::APPLICATION_INSTALL_REASON_SYNC;
    case InstallReason::kUser:
      return ApplicationInstallReason::APPLICATION_INSTALL_REASON_USER;
    case InstallReason::kSubApp:
      return ApplicationInstallReason::APPLICATION_INSTALL_REASON_SUB_APP;
    case InstallReason::kKiosk:
      return ApplicationInstallReason::APPLICATION_INSTALL_REASON_KIOSK;
    case InstallReason::kCommandLine:
      return ApplicationInstallReason::APPLICATION_INSTALL_REASON_COMMAND_LINE;
  }
}

ApplicationInstallSource ConvertInstallSourceToProtoApplicationInstallSource(
    InstallSource install_source) {
  switch (install_source) {
    case InstallSource::kUnknown:
      return ApplicationInstallSource::APPLICATION_INSTALL_SOURCE_UNKNOWN;
    case InstallSource::kSystem:
      return ApplicationInstallSource::APPLICATION_INSTALL_SOURCE_SYSTEM;
    case InstallSource::kSync:
      return ApplicationInstallSource::APPLICATION_INSTALL_SOURCE_SYNC;
    case InstallSource::kPlayStore:
      return ApplicationInstallSource::APPLICATION_INSTALL_SOURCE_PLAY_STORE;
    case InstallSource::kChromeWebStore:
      return ApplicationInstallSource::
          APPLICATION_INSTALL_SOURCE_CHROME_WEB_STORE;
    case InstallSource::kBrowser:
      return ApplicationInstallSource::APPLICATION_INSTALL_SOURCE_BROWSER;
  }
}

ApplicationUninstallSource
ConvertUninstallSourceToProtoApplicationUninstallSource(
    UninstallSource uninstall_source) {
  switch (uninstall_source) {
    case UninstallSource::kUnknown:
      return ApplicationUninstallSource::APPLICATION_UNINSTALL_SOURCE_UNKNOWN;
    case UninstallSource::kAppList:
      return ApplicationUninstallSource::APPLICATION_UNINSTALL_SOURCE_APP_LIST;
    case UninstallSource ::kAppManagement:
      return ApplicationUninstallSource::
          APPLICATION_UNINSTALL_SOURCE_APP_MANAGEMENT;
    case UninstallSource ::kShelf:
      return ApplicationUninstallSource::APPLICATION_UNINSTALL_SOURCE_SHELF;
    case UninstallSource ::kMigration:
      return ApplicationUninstallSource::APPLICATION_UNINSTALL_SOURCE_MIGRATION;
  }
}

}  // namespace apps
