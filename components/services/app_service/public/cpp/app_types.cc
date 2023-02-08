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
    app->icon_key = std::move(*icon_key->Clone());
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

}  // namespace apps
