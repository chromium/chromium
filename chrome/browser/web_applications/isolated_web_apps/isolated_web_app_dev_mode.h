// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_DEV_MODE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_DEV_MODE_H_

#include "base/strings/string_piece.h"

class PrefService;

namespace web_app {

constexpr inline base::StringPiece kIwaDevModeNotEnabledMessage =
    "Isolated Web Apps are not enabled, or Isolated Web App Developer Mode is "
    "not enabled or blocked by policy.";

bool IsIwaDevModeEnabled(const PrefService& pref_service);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_DEV_MODE_H_
