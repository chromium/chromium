// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_FEATURES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_FEATURES_H_

#include <string_view>

class Profile;

namespace web_app {

constexpr inline std::string_view kIwaDevModeNotEnabledMessage =
    "Isolated Web Apps are not enabled, or Isolated Web App Developer Mode is "
    "not enabled or blocked by policy.";

bool IsIwaDevModeEnabled(Profile* profile);

bool IsIwaUnmanagedInstallEnabled(Profile* profile);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_FEATURES_H_
