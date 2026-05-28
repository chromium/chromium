// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_management_type.h"

#include <ostream>

#include "base/containers/enum_set.h"

namespace web_app {

static_assert(WebAppManagement::kMinValue == 0,
              "Source enum should be zero based");

static_assert(WebAppManagementTypes::All().Has(WebAppManagement::kMinValue),
              "WebAppManagementTypes is missing an enum value");
static_assert(WebAppManagementTypes::All().Has(WebAppManagement::kMaxValue),
              "WebAppManagementTypes is missing an enum value");
static_assert(WebAppManagementTypes::kValueCount ==
                  WebAppManagement::kMaxValue + 1,
              "WebAppManagementTypes is missing an enum value");

// WebAppManagement types that can't be uninstalled by the user. Counterpart to
// kUserUninstallableSources.
constexpr WebAppManagementTypes kNotUserUninstallableSources = {
    WebAppManagement::kSystem,
    WebAppManagement::kKiosk,
    WebAppManagement::kPolicy,
    WebAppManagement::kIwaPolicy,
};

// When adding a new WebAppManagement::Type, mark whether or not it is
// uninstallable by adding it to the appropriate WebAppManagementTypes constant.
static_assert(base::Union(kUserUninstallableSources,
                          kNotUserUninstallableSources) ==
                  WebAppManagementTypes::All(),
              "Each WebAppManagement::Type must be listed in either "
              "web_app::kUserUninstallableSources or "
              "web_app::kNotUserUninstallableSources");
static_assert(base::Intersection(kUserUninstallableSources,
                                 kNotUserUninstallableSources)
                  .empty(),
              "No WebAppManagement::Type may appear in both "
              "web_app::kUserUninstallableSources and "
              "web_app::kNotUserUninstallableSources");

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
    case WebAppManagement::Type::kUserInstalled:
      return os << "UserInstalled";
    case WebAppManagement::Type::kIwaShimlessRma:
      return os << "IwaShimlessRma";
    case WebAppManagement::Type::kIwaPolicy:
      return os << "IwaPolicy";
    case WebAppManagement::Type::kIwaUserInstalled:
      return os << "IwaUserInstalled";
    case WebAppManagement::Type::kApsDefault:
      return os << "ApsDefault";
    case WebAppManagement::Type::kDefault:
      return os << "Default";
  }
}

bool IsIwaType(WebAppManagement::Type type) {
  switch (type) {
    case WebAppManagement::kSystem:
    case WebAppManagement::kKiosk:
    case WebAppManagement::kPolicy:
    case WebAppManagement::kOem:
    case WebAppManagement::kSubApp:
    case WebAppManagement::kWebAppStore:
    case WebAppManagement::kOneDriveIntegration:
    case WebAppManagement::kSync:
    case WebAppManagement::kUserInstalled:
    case WebAppManagement::kApsDefault:
    case WebAppManagement::kDefault:
      return false;
    case WebAppManagement::kIwaPolicy:
    case WebAppManagement::kIwaShimlessRma:
    case WebAppManagement::kIwaUserInstalled:
      return true;
  }
}

}  // namespace WebAppManagement
}  // namespace web_app
