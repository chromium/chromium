// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_constants.h"

#include <ostream>

namespace web_app {

const char kRunOnOsLoginModeWindowed[] = "windowed";

static_assert(WebAppManagement::kMinValue == 0,
              "Source enum should be zero based");

std::ostream& operator<<(std::ostream& os, WebAppManagement::Type type) {
  switch (type) {
    case WebAppManagement::Type::kSystem:
      return os << "System";
    case WebAppManagement::Type::kPolicy:
      return os << "Policy";
    case WebAppManagement::Type::kSubApp:
      return os << "SubApp";
    case WebAppManagement::Type::kWebAppStore:
      return os << "WebAppStore";
    case WebAppManagement::Type::kSync:
      return os << "Sync";
    case WebAppManagement::Type::kDefault:
      return os << "Default";
  }
}

static_assert(OsHookType::kShortcuts == 0,
              "OsHookType enum should be zero based");

}  // namespace web_app
