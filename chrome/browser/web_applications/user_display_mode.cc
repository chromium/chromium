// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <type_traits>

#include "chrome/browser/web_applications/user_display_mode.h"

#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync/protocol/web_app_specifics.pb.h"

namespace web_app {

bool operator==(UserDisplayMode lhs, UserDisplayMode rhs) {
  return static_cast<std::underlying_type<UserDisplayMode>::type>(lhs) ==
         static_cast<std::underlying_type<UserDisplayMode>::type>(rhs);
}

bool operator!=(UserDisplayMode lhs, UserDisplayMode rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, UserDisplayMode user_display_mode) {
  return os << ConvertUserDisplayModeToString(user_display_mode);
}

std::string ConvertUserDisplayModeToString(UserDisplayMode user_display_mode) {
  switch (user_display_mode) {
    case UserDisplayMode::kBrowser:
      return "browser";
    case UserDisplayMode::kStandalone:
      return "standalone";
    case UserDisplayMode::kTabbed:
      return "tabbed";
  }
}

::sync_pb::WebAppSpecifics::UserDisplayMode
ConvertUserDisplayModeToWebAppSpecificsUserDisplayMode(
    UserDisplayMode user_display_mode) {
  switch (user_display_mode) {
    case UserDisplayMode::kBrowser:
      return ::sync_pb::WebAppSpecifics::BROWSER;
    case UserDisplayMode::kTabbed:
      return ::sync_pb::WebAppSpecifics::TABBED;
    case UserDisplayMode::kStandalone:
      return ::sync_pb::WebAppSpecifics::STANDALONE;
  }
}

UserDisplayMode CreateUserDisplayModeFromWebAppSpecificsUserDisplayMode(
    ::sync_pb::WebAppSpecifics::UserDisplayMode display_mode) {
  switch (display_mode) {
    case ::sync_pb::WebAppSpecifics::BROWSER:
      return UserDisplayMode::kBrowser;
    case ::sync_pb::WebAppSpecifics::TABBED:
      return UserDisplayMode::kTabbed;
    case ::sync_pb::WebAppSpecifics::STANDALONE:
      return UserDisplayMode::kStandalone;
    case ::sync_pb::WebAppSpecifics::UNSPECIFIED:
      // The same as `ToMojomDisplayMode`.
      return UserDisplayMode::kStandalone;
  }
}

}  // namespace web_app
