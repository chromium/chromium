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

static_assert(WebAppManagementTypes::All().Has(WebAppManagement::kMinValue),
              "WebAppManagementTypes is missing an enum value");
static_assert(WebAppManagementTypes::All().Has(WebAppManagement::kMaxValue),
              "WebAppManagementTypes is missing an enum value");
static_assert(WebAppManagementTypes::kValueCount ==
                  WebAppManagement::kMaxValue + 1,
              "WebAppManagementTypes is missing an enum value");

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

std::ostream& operator<<(std::ostream& os, RunOnOsLoginMode mode) {
  switch (mode) {
    case RunOnOsLoginMode::kWindowed:
      return os << "windowed";
    case RunOnOsLoginMode::kMinimized:
      return os << "minimized";
    case RunOnOsLoginMode::kNotRun:
      return os << "not run";
  }
}

std::ostream& operator<<(std::ostream& os, ApiApprovalState state) {
  switch (state) {
    case ApiApprovalState::kAllowed:
      return os << "Allowed";
    case ApiApprovalState::kDisallowed:
      return os << "Disallowed";
    case ApiApprovalState::kRequiresPrompt:
      return os << "RequiresPrompt";
  }
}

}  // namespace web_app
