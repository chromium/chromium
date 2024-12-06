// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

std::ostream& operator<<(std::ostream& os, AppType v) {
  switch (v) {
    case AppType::kUnknown:
      return os << "AppType::kUnknown";
    case AppType::kArc:
      return os << "AppType::kArc";
    case AppType::kCrostini:
      return os << "AppType::kCrostini";
    case AppType::kChromeApp:
      return os << "AppType::kChromeApp";
    case AppType::kWeb:
      return os << "AppType::kWeb";
    case AppType::kPluginVm:
      return os << "AppType::kPluginVm";
    case AppType::kRemote:
      return os << "AppType::kRemote";
    case AppType::kBorealis:
      return os << "AppType::kBorealis";
    case AppType::kSystemWeb:
      return os << "AppType::kSystemWeb";
    case AppType::kExtension:
      return os << "AppType::kExtension";
    case AppType::kBruschetta:
      return os << "AppType::kBruschetta";
  }

  // Just in case, where the value comes from outside of the chrome code
  // then casted without checks.
  return os << "(unknown: " << static_cast<int>(v) << ")";
}

std::ostream& operator<<(std::ostream& os, PackageType v) {
  switch (v) {
    case PackageType::kUnknown:
      return os << "PackageType::kUnknown";
    case PackageType::kArc:
      return os << "PackageType::kArc";
    case PackageType::kBorealis:
      return os << "PackageType::kBorealis";
    case PackageType::kChromeApp:
      return os << "PackageType::kChromeApp";
    case PackageType::kGeForceNow:
      return os << "PackageType::kGeForceNow";
    case PackageType::kSystem:
      return os << "PackageType::kSystem";
    case PackageType::kWeb:
      return os << "PackageType::kWeb";
    case PackageType::kWebsite:
      return os << "PackageType::kWebsite";
  }

  // Just in case, where the value comes from outside of the chrome code
  // then casted without checks.
  return os << "(unknown: " << static_cast<int>(v) << ")";
}

std::ostream& operator<<(std::ostream& os, Readiness v) {
  switch (v) {
    case Readiness::kUnknown:
      return os << "Readiness::kUnknown";
    case Readiness::kReady:
      return os << "Readiness::kReady";
    case Readiness::kDisabledByBlocklist:
      return os << "Readiness::KDisabledByBlocklist";
    case Readiness::kDisabledByPolicy:
      return os << "Readiness::kDisabledByPolicy";
    case Readiness::kDisabledByUser:
      return os << "Readiness::kDisabledByUser";
    case Readiness::kTerminated:
      return os << "Readiness::kTerminated";
    case Readiness::kUninstalledByUser:
      return os << "Readiness::kUninstalledByUser";
    case Readiness::kRemoved:
      return os << "Readiness::kRemoved";
    case Readiness::kUninstalledByNonUser:
      return os << "Readiness::kUninstalledByNonUser";
    case Readiness::kDisabledByLocalSettings:
      return os << "Readiness::kDisabledByLocalSettings";
  }

  // Just in case, where the value comes from outside of the chrome code
  // then casted without checks.
  return os << "(unknown: " << static_cast<int>(v) << ")";
}

std::ostream& operator<<(std::ostream& os, InstallReason v) {
  switch (v) {
    case InstallReason::kUnknown:
      return os << "InstallReason::kUnknown";
    case InstallReason::kSystem:
      return os << "InstallReason::kSystem";
    case InstallReason::kPolicy:
      return os << "InstallReason::kPolicy";
    case InstallReason::kOem:
      return os << "InstallReason::kOem";
    case InstallReason::kDefault:
      return os << "InstallReason::kDefault";
    case InstallReason::kSync:
      return os << "InstallReason::kSync";
    case InstallReason::kUser:
      return os << "InstallReason::kUser";
    case InstallReason::kSubApp:
      return os << "InstallReason::SubApp";
    case InstallReason::kKiosk:
      return os << "InstallReason::kKiosk";
    case InstallReason::kCommandLine:
      return os << "InstallReason::kCommandLine";
  }

  // Just in case, where the value comes from outside of the chrome code
  // then casted without checks.
  return os << "(unknown: " << static_cast<int>(v) << ")";
}

std::ostream& operator<<(std::ostream& os, InstallSource v) {
  switch (v) {
    case InstallSource::kUnknown:
      return os << "InstallSource::kUnknown";
    case InstallSource::kSystem:
      return os << "InstallSource::kSystem";
    case InstallSource::kSync:
      return os << "InstallSource::kSync";
    case InstallSource::kPlayStore:
      return os << "InstallSource::kPlayStore";
    case InstallSource::kChromeWebStore:
      return os << "InstallSource::kChromeWebStore";
    case InstallSource::kBrowser:
      return os << "InstallSource::kBrowser";
  }

  // Just in case, where the value comes from outside of the chrome code
  // then casted without checks.
  return os << "(unknown: " << static_cast<int>(v) << ")";
}

std::ostream& operator<<(std::ostream& os, WindowMode v) {
  switch (v) {
    case WindowMode::kUnknown:
      return os << "WindowMode::kUnknown";
    case WindowMode::kWindow:
      return os << "WindowMode::kWindow";
    case WindowMode::kBrowser:
      return os << "WindowMode::kBrowser";
    case WindowMode::kTabbedWindow:
      return os << "WindowMode::kTabbedWindow";
  }

  // Just in case, where the value comes from outside of the chrome code
  // then casted without checks.
  return os << "(unknown: " << static_cast<int>(v) << ")";
}

ApplicationType ConvertAppTypeToProtoApplicationType(AppType app_type) {
  switch (app_type) {
    case AppType::kUnknown:
      return ApplicationType::APPLICATION_TYPE_UNKNOWN;
    case AppType::kArc:
      return ApplicationType::APPLICATION_TYPE_ARC;
    case AppType::kCrostini:
      return ApplicationType::APPLICATION_TYPE_CROSTINI;
    case AppType::kChromeApp:
      return ApplicationType::APPLICATION_TYPE_CHROME_APP;
    case AppType::kWeb:
      return ApplicationType::APPLICATION_TYPE_WEB;
    case AppType::kPluginVm:
      return ApplicationType::APPLICATION_TYPE_PLUGIN_VM;
    case AppType::kRemote:
      return ApplicationType::APPLICATION_TYPE_REMOTE;
    case AppType::kBorealis:
      return ApplicationType::APPLICATION_TYPE_BOREALIS;
    case AppType::kSystemWeb:
      return ApplicationType::APPLICATION_TYPE_SYSTEM_WEB;
    case AppType::kExtension:
      return ApplicationType::APPLICATION_TYPE_EXTENSION;
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
    case AppType::kCrostini:
    case AppType::kPluginVm:
    case AppType::kRemote:
    case AppType::kSystemWeb:
    case AppType::kExtension:
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
