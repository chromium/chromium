// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_constants.h"

#include <ostream>
#include <string>

#include "components/webapps/browser/installable/installable_metrics.h"

namespace web_app {

const char kRunOnOsLoginModeWindowed[] = "windowed";

static_assert(WebAppManagement::kMinValue == 0,
              "Source enum should be zero based");

namespace WebAppManagement {
std::ostream& operator<<(std::ostream& os, WebAppManagement::Type type) {
  switch (type) {
    case WebAppManagement::Type::kSystem:
      return os << "System";
    case WebAppManagement::Type::kKiosk:
      return os << "Kiosk";
    case WebAppManagement::Type::kPolicy:
      return os << "Policy";
    case WebAppManagement::Type::kOem:
      return os << "OEM";
    case WebAppManagement::Type::kSubApp:
      return os << "SubApp";
    case WebAppManagement::Type::kWebAppStore:
      return os << "WebAppStore";
    case WebAppManagement::Type::kOneDriveIntegration:
      return os << "OneDriveIntegration";
    case WebAppManagement::Type::kSync:
      return os << "Sync";
    case WebAppManagement::Type::kDefault:
      return os << "Default";
    case WebAppManagement::Type::kCommandLine:
      return os << "CommandLine";
  }
}
}  // namespace WebAppManagement

static_assert(OsHookType::kShortcuts == 0,
              "OsHookType enum should be zero based");

std::string ConvertUninstallSourceToStringType(
    const webapps::WebappUninstallSource& uninstall_source) {
  switch (uninstall_source) {
    case webapps::WebappUninstallSource::kUnknown:
      return "Unknown";
    case webapps::WebappUninstallSource::kAppMenu:
      return "AppMenu";
    case webapps::WebappUninstallSource::kAppsPage:
      return "AppsPage";
    case webapps::WebappUninstallSource::kOsSettings:
      return "OS Settings";
    case webapps::WebappUninstallSource::kSync:
      return "Sync";
    case webapps::WebappUninstallSource::kAppManagement:
      return "App Management";
    case webapps::WebappUninstallSource::kMigration:
      return "Migration";
    case webapps::WebappUninstallSource::kAppList:
      return "App List";
    case webapps::WebappUninstallSource::kShelf:
      return "Shelf";
    case webapps::WebappUninstallSource::kInternalPreinstalled:
      return "Internal Preinstalled";
    case webapps::WebappUninstallSource::kExternalPreinstalled:
      return "External Preinstalled";
    case webapps::WebappUninstallSource::kExternalPolicy:
      return "External Policy";
    case webapps::WebappUninstallSource::kSystemPreinstalled:
      return "System Preinstalled";
    case webapps::WebappUninstallSource::kPlaceholderReplacement:
      return "Placeholder Replacement";
    case webapps::WebappUninstallSource::kArc:
      return "Arc";
    case webapps::WebappUninstallSource::kSubApp:
      return "SubApp";
    case webapps::WebappUninstallSource::kStartupCleanup:
      return "Startup Cleanup";
    case webapps::WebappUninstallSource::kParentUninstall:
      return "Parent App Uninstalled";
    case webapps::WebappUninstallSource::kExternalLockScreen:
      return "External Lock Screen";
    case webapps::WebappUninstallSource::kTestCleanup:
      return "Test cleanup";
  }
}

}  // namespace web_app
