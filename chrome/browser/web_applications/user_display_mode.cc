// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <type_traits>

#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync/protocol/web_app_specifics.pb.h"

namespace web_app {

std::string ConvertUserDisplayModeToString(
    mojom::UserDisplayMode user_display_mode) {
  switch (user_display_mode) {
    case mojom::UserDisplayMode::kBrowser:
      return "browser";
    case mojom::UserDisplayMode::kStandalone:
      return "standalone";
    case mojom::UserDisplayMode::kTabbed:
      return "tabbed";
  }
}

::sync_pb::WebAppSpecifics::UserDisplayMode
ConvertUserDisplayModeToWebAppSpecificsUserDisplayMode(
    mojom::UserDisplayMode user_display_mode) {
  switch (user_display_mode) {
    case mojom::UserDisplayMode::kBrowser:
      return ::sync_pb::WebAppSpecifics::BROWSER;
    case mojom::UserDisplayMode::kTabbed:
      return ::sync_pb::WebAppSpecifics::TABBED;
    case mojom::UserDisplayMode::kStandalone:
      return ::sync_pb::WebAppSpecifics::STANDALONE;
  }
}

mojom::UserDisplayMode CreateUserDisplayModeFromWebAppSpecificsUserDisplayMode(
    ::sync_pb::WebAppSpecifics::UserDisplayMode display_mode) {
  switch (display_mode) {
    case ::sync_pb::WebAppSpecifics::BROWSER:
      return mojom::UserDisplayMode::kBrowser;
    case ::sync_pb::WebAppSpecifics::TABBED:
      return mojom::UserDisplayMode::kTabbed;
    case ::sync_pb::WebAppSpecifics::STANDALONE:
      return mojom::UserDisplayMode::kStandalone;
    case ::sync_pb::WebAppSpecifics::UNSPECIFIED:
      // The same as `ToMojomDisplayMode`.
      return mojom::UserDisplayMode::kStandalone;
  }
}

}  // namespace web_app
