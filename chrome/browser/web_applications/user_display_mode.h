// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_USER_DISPLAY_MODE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_USER_DISPLAY_MODE_H_

#include <ostream>
#include <string>

#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync/protocol/web_app_specifics.pb.h"

namespace web_app {

// Reflects
// https://source.chromium.org/chromium/chromium/src/+/main:components/sync/protocol/web_app_specifics.proto;l=44;drc=0758d11f3d4db21e8e8b1b3a5600261fde150afb
enum class UserDisplayMode {
  // The user prefers that the web app open in a browser context, like a tab.
  kBrowser,

  // The user prefers the web app open in a standalone window experience.
  kStandalone,

  // The user wants the web app to open in a standalone window experience with
  // tabs.
  kTabbed,
};

bool operator==(UserDisplayMode lhs, UserDisplayMode rhs);
bool operator!=(UserDisplayMode lhs, UserDisplayMode rhs);

std::ostream& operator<<(std::ostream& os, UserDisplayMode user_display_mode);

std::string ConvertUserDisplayModeToString(UserDisplayMode user_display_mode);

::sync_pb::WebAppSpecifics::UserDisplayMode
ConvertUserDisplayModeToWebAppSpecificsUserDisplayMode(
    UserDisplayMode user_display_mode);

UserDisplayMode CreateUserDisplayModeFromWebAppSpecificsUserDisplayMode(
    ::sync_pb::WebAppSpecifics::UserDisplayMode display_mode);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_USER_DISPLAY_MODE_H_
