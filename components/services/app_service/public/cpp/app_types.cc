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
                   kMacOs,
                   kPluginVm,
                   kStandaloneBrowser,
                   kRemote,
                   kBorealis,
                   kSystemWeb,
                   kStandaloneBrowserChromeApp,
                   kExtension,
                   kStandaloneBrowserExtension,
                   kBruschetta)
APP_ENUM_TO_STRING(Readiness,
                   kUnknown,
                   kReady,
                   kDisabledByBlocklist,
                   kDisabledByPolicy,
                   kDisabledByUser,
                   kTerminated,
                   kUninstalledByUser,
                   kRemoved,
                   kUninstalledByMigration)
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

App::App(AppType app_type, const std::string& app_id)
    : app_type(app_type), app_id(app_id) {}

App::~App() = default;

AppPtr App::Clone() const {
  auto app = std::make_unique<App>(app_type, app_id);

  app->readiness = readiness;
  app->name = name;
  app->short_name = short_name;
  app->publisher_id = publisher_id;
  app->description = description;
  app->version = version;
  app->additional_search_terms = additional_search_terms;

  if (icon_key.has_value()) {
    app->icon_key = apps::IconKey(icon_key->timeline, icon_key->resource_id,
                                  icon_key->icon_effects);
  }

  app->last_launch_time = last_launch_time;
  app->install_time = install_time;
  app->permissions = ClonePermissions(permissions);
  app->install_reason = install_reason;
  app->install_source = install_source;
  app->policy_ids = policy_ids;
  app->is_platform_app = is_platform_app;
  app->recommendable = recommendable;
  app->searchable = searchable;
  app->show_in_launcher = show_in_launcher;
  app->show_in_shelf = show_in_shelf;
  app->show_in_search = show_in_search;
  app->show_in_management = show_in_management;
  app->handles_intents = handles_intents;
  app->allow_uninstall = allow_uninstall;
  app->has_badge = has_badge;
  app->paused = paused;
  app->intent_filters = CloneIntentFilters(intent_filters);
  app->resize_locked = resize_locked;
  app->window_mode = window_mode;

  if (run_on_os_login.has_value()) {
    app->run_on_os_login = apps::RunOnOsLogin(run_on_os_login->login_mode,
                                              run_on_os_login->is_managed);
  }

  app->shortcuts = CloneShortcuts(shortcuts);

  app->app_size_in_bytes = app_size_in_bytes;
  app->data_size_in_bytes = data_size_in_bytes;

  return app;
}

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
    case AppType::kMacOs:
      return ApplicationType::APPLICATION_TYPE_MAC_OS;
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

AppType ConvertMojomAppTypToAppType(apps::mojom::AppType mojom_app_type) {
  switch (mojom_app_type) {
    case apps::mojom::AppType::kUnknown:
      return AppType::kUnknown;
    case apps::mojom::AppType::kArc:
      return AppType::kArc;
    case apps::mojom::AppType::kBuiltIn:
      return AppType::kBuiltIn;
    case apps::mojom::AppType::kCrostini:
      return AppType::kCrostini;
    case apps::mojom::AppType::kChromeApp:
      return AppType::kChromeApp;
    case apps::mojom::AppType::kWeb:
      return AppType::kWeb;
    case apps::mojom::AppType::kMacOs:
      return AppType::kMacOs;
    case apps::mojom::AppType::kPluginVm:
      return AppType::kPluginVm;
    case apps::mojom::AppType::kStandaloneBrowser:
      return AppType::kStandaloneBrowser;
    case apps::mojom::AppType::kRemote:
      return AppType::kRemote;
    case apps::mojom::AppType::kBorealis:
      return AppType::kBorealis;
    case apps::mojom::AppType::kSystemWeb:
      return AppType::kSystemWeb;
    case apps::mojom::AppType::kStandaloneBrowserChromeApp:
      return AppType::kStandaloneBrowserChromeApp;
    case apps::mojom::AppType::kExtension:
      return AppType::kExtension;
    case apps::mojom::AppType::kStandaloneBrowserExtension:
      return AppType::kStandaloneBrowserExtension;
    case apps::mojom::AppType::kBruschetta:
      return AppType::kBruschetta;
  }
}

mojom::AppType ConvertAppTypeToMojomAppType(AppType app_type) {
  switch (app_type) {
    case AppType::kUnknown:
      return apps::mojom::AppType::kUnknown;
    case AppType::kArc:
      return apps::mojom::AppType::kArc;
    case AppType::kBuiltIn:
      return apps::mojom::AppType::kBuiltIn;
    case AppType::kCrostini:
      return apps::mojom::AppType::kCrostini;
    case AppType::kChromeApp:
      return apps::mojom::AppType::kChromeApp;
    case AppType::kWeb:
      return apps::mojom::AppType::kWeb;
    case AppType::kMacOs:
      return apps::mojom::AppType::kMacOs;
    case AppType::kPluginVm:
      return apps::mojom::AppType::kPluginVm;
    case AppType::kStandaloneBrowser:
      return apps::mojom::AppType::kStandaloneBrowser;
    case AppType::kRemote:
      return apps::mojom::AppType::kRemote;
    case AppType::kBorealis:
      return apps::mojom::AppType::kBorealis;
    case AppType::kSystemWeb:
      return apps::mojom::AppType::kSystemWeb;
    case AppType::kStandaloneBrowserChromeApp:
      return apps::mojom::AppType::kStandaloneBrowserChromeApp;
    case AppType::kExtension:
      return apps::mojom::AppType::kExtension;
    case AppType::kStandaloneBrowserExtension:
      return apps::mojom::AppType::kStandaloneBrowserExtension;
    case AppType::kBruschetta:
      return apps::mojom::AppType::kBruschetta;
  }
}

Readiness ConvertMojomReadinessToReadiness(
    apps::mojom::Readiness mojom_readiness) {
  switch (mojom_readiness) {
    case apps::mojom::Readiness::kUnknown:
      return Readiness::kUnknown;
    case apps::mojom::Readiness::kReady:
      return Readiness::kReady;
    case apps::mojom::Readiness::kDisabledByBlocklist:
      return Readiness::kDisabledByBlocklist;
    case apps::mojom::Readiness::kDisabledByPolicy:
      return Readiness::kDisabledByPolicy;
    case apps::mojom::Readiness::kDisabledByUser:
      return Readiness::kDisabledByUser;
    case apps::mojom::Readiness::kTerminated:
      return Readiness::kTerminated;
    case apps::mojom::Readiness::kUninstalledByUser:
      return Readiness::kUninstalledByUser;
    case apps::mojom::Readiness::kRemoved:
      return Readiness::kRemoved;
    case apps::mojom::Readiness::kUninstalledByMigration:
      return Readiness::kUninstalledByMigration;
  }
}

apps::mojom::Readiness ConvertReadinessToMojomReadiness(Readiness readiness) {
  switch (readiness) {
    case Readiness::kUnknown:
      return apps::mojom::Readiness::kUnknown;
    case Readiness::kReady:
      return apps::mojom::Readiness::kReady;
    case Readiness::kDisabledByBlocklist:
      return apps::mojom::Readiness::kDisabledByBlocklist;
    case Readiness::kDisabledByPolicy:
      return apps::mojom::Readiness::kDisabledByPolicy;
    case Readiness::kDisabledByUser:
      return apps::mojom::Readiness::kDisabledByUser;
    case Readiness::kTerminated:
      return apps::mojom::Readiness::kTerminated;
    case Readiness::kUninstalledByUser:
      return apps::mojom::Readiness::kUninstalledByUser;
    case Readiness::kRemoved:
      return apps::mojom::Readiness::kRemoved;
    case Readiness::kUninstalledByMigration:
      return apps::mojom::Readiness::kUninstalledByMigration;
  }
}

InstallReason ConvertMojomInstallReasonToInstallReason(
    apps::mojom::InstallReason mojom_install_reason) {
  switch (mojom_install_reason) {
    case apps::mojom::InstallReason::kUnknown:
      return InstallReason::kUnknown;
    case apps::mojom::InstallReason::kSystem:
      return InstallReason::kSystem;
    case apps::mojom::InstallReason::kPolicy:
      return InstallReason::kPolicy;
    case apps::mojom::InstallReason::kOem:
      return InstallReason::kOem;
    case apps::mojom::InstallReason::kDefault:
      return InstallReason::kDefault;
    case apps::mojom::InstallReason::kSync:
      return InstallReason::kSync;
    case apps::mojom::InstallReason::kUser:
      return InstallReason::kUser;
    case apps::mojom::InstallReason::kSubApp:
      return InstallReason::kSubApp;
    case apps::mojom::InstallReason::kKiosk:
      return InstallReason::kKiosk;
    case apps::mojom::InstallReason::kCommandLine:
      return InstallReason::kCommandLine;
  }
}

apps::mojom::InstallReason ConvertInstallReasonToMojomInstallReason(
    InstallReason install_reason) {
  switch (install_reason) {
    case InstallReason::kUnknown:
      return apps::mojom::InstallReason::kUnknown;
    case InstallReason::kSystem:
      return apps::mojom::InstallReason::kSystem;
    case InstallReason::kPolicy:
      return apps::mojom::InstallReason::kPolicy;
    case InstallReason::kOem:
      return apps::mojom::InstallReason::kOem;
    case InstallReason::kDefault:
      return apps::mojom::InstallReason::kDefault;
    case InstallReason::kSync:
      return apps::mojom::InstallReason::kSync;
    case InstallReason::kUser:
      return apps::mojom::InstallReason::kUser;
    case InstallReason::kSubApp:
      return apps::mojom::InstallReason::kSubApp;
    case InstallReason::kKiosk:
      return apps::mojom::InstallReason::kKiosk;
    case InstallReason::kCommandLine:
      return apps::mojom::InstallReason::kCommandLine;
  }
}

InstallSource ConvertMojomInstallSourceToInstallSource(
    apps::mojom::InstallSource mojom_install_source) {
  switch (mojom_install_source) {
    case apps::mojom::InstallSource::kUnknown:
      return InstallSource::kUnknown;
    case apps::mojom::InstallSource::kSystem:
      return InstallSource::kSystem;
    case apps::mojom::InstallSource::kSync:
      return InstallSource::kSync;
    case apps::mojom::InstallSource::kPlayStore:
      return InstallSource::kPlayStore;
    case apps::mojom::InstallSource::kChromeWebStore:
      return InstallSource::kChromeWebStore;
    case apps::mojom::InstallSource::kBrowser:
      return InstallSource::kBrowser;
  }
}

apps::mojom::InstallSource ConvertInstallSourceToMojomInstallSource(
    InstallSource install_source) {
  switch (install_source) {
    case InstallSource::kUnknown:
      return apps::mojom::InstallSource::kUnknown;
    case InstallSource::kSystem:
      return apps::mojom::InstallSource::kSystem;
    case InstallSource::kSync:
      return apps::mojom::InstallSource::kSync;
    case InstallSource::kPlayStore:
      return apps::mojom::InstallSource::kPlayStore;
    case InstallSource::kChromeWebStore:
      return apps::mojom::InstallSource::kChromeWebStore;
    case InstallSource::kBrowser:
      return apps::mojom::InstallSource::kBrowser;
  }
}

WindowMode ConvertMojomWindowModeToWindowMode(
    apps::mojom::WindowMode mojom_window_mode) {
  switch (mojom_window_mode) {
    case apps::mojom::WindowMode::kUnknown:
      return WindowMode::kUnknown;
    case apps::mojom::WindowMode::kWindow:
      return WindowMode::kWindow;
    case apps::mojom::WindowMode::kBrowser:
      return WindowMode::kBrowser;
    case apps::mojom::WindowMode::kTabbedWindow:
      return WindowMode::kTabbedWindow;
  }
}

apps::RunOnOsLoginMode ConvertMojomRunOnOsLoginModeToRunOnOsLoginMode(
    apps::mojom::RunOnOsLoginMode run_on_os_login_mode) {
  switch (run_on_os_login_mode) {
    case apps::mojom::RunOnOsLoginMode::kUnknown:
      return apps::RunOnOsLoginMode::kUnknown;
    case apps::mojom::RunOnOsLoginMode::kWindowed:
      return apps::RunOnOsLoginMode::kWindowed;
    case apps::mojom::RunOnOsLoginMode::kNotRun:
      return apps::RunOnOsLoginMode::kNotRun;
  }
}

apps::mojom::WindowMode ConvertWindowModeToMojomWindowMode(
    WindowMode window_mode) {
  switch (window_mode) {
    case WindowMode::kUnknown:
      return apps::mojom::WindowMode::kUnknown;
    case WindowMode::kWindow:
      return apps::mojom::WindowMode::kWindow;
    case WindowMode::kBrowser:
      return apps::mojom::WindowMode::kBrowser;
    case WindowMode::kTabbedWindow:
      return apps::mojom::WindowMode::kTabbedWindow;
  }
}

absl::optional<bool> GetOptionalBool(
    const apps::mojom::OptionalBool& mojom_optional_bool) {
  absl::optional<bool> optional_bool;
  if (mojom_optional_bool != apps::mojom::OptionalBool::kUnknown) {
    optional_bool = mojom_optional_bool == apps::mojom::OptionalBool::kTrue;
  }
  return optional_bool;
}

apps::mojom::OptionalBool GetMojomOptionalBool(
    const absl::optional<bool>& optional_bool) {
  return optional_bool.has_value()
             ? (optional_bool.value() ? apps::mojom::OptionalBool::kTrue
                                      : apps::mojom::OptionalBool::kFalse)
             : apps::mojom::OptionalBool::kUnknown;
}

std::vector<base::FilePath> ConvertMojomFilePathsToFilePaths(
    apps::mojom::FilePathsPtr mojom_file_paths) {
  std::vector<base::FilePath> file_paths;
  if (mojom_file_paths) {
    file_paths = std::move(mojom_file_paths->file_paths);
  }
  return file_paths;
}

}  // namespace apps
