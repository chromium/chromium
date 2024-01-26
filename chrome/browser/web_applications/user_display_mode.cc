// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/user_display_mode.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/features.h"
#include "components/sync/protocol/web_app_specifics.pb.h"

namespace web_app {

sync_pb::WebAppSpecifics::UserDisplayMode
ConvertUserDisplayModeToWebAppSpecificsUserDisplayMode(
    mojom::UserDisplayMode user_display_mode) {
  switch (user_display_mode) {
    case mojom::UserDisplayMode::kBrowser:
      return sync_pb::WebAppSpecifics::BROWSER;
    case mojom::UserDisplayMode::kTabbed:
      return sync_pb::WebAppSpecifics::TABBED;
    case mojom::UserDisplayMode::kStandalone:
      return sync_pb::WebAppSpecifics::STANDALONE;
  }
}

mojom::UserDisplayMode CreateUserDisplayModeFromWebAppSpecificsUserDisplayMode(
    sync_pb::WebAppSpecifics::UserDisplayMode display_mode) {
  switch (display_mode) {
    case sync_pb::WebAppSpecifics::BROWSER:
      return mojom::UserDisplayMode::kBrowser;
    case sync_pb::WebAppSpecifics::TABBED:
      return mojom::UserDisplayMode::kTabbed;
    case sync_pb::WebAppSpecifics::STANDALONE:
    case sync_pb::WebAppSpecifics::UNSPECIFIED:
      // Default to standalone if it's an enum value we don't know about.
      return mojom::UserDisplayMode::kStandalone;
  }
}

mojom::UserDisplayMode ResolvePlatformSpecificUserDisplayMode(
    const sync_pb::WebAppSpecifics& sync_proto) {
  if (!base::FeatureList::IsEnabled(kSeparateUserDisplayModeForCrOS)) {
    return CreateUserDisplayModeFromWebAppSpecificsUserDisplayMode(
        sync_proto.user_display_mode_non_cros());
  }

  sync_pb::WebAppSpecifics_UserDisplayMode user_display_mode;
#if BUILDFLAG(IS_CHROMEOS)
  user_display_mode = sync_proto.has_user_display_mode_cros()
                          ? sync_proto.user_display_mode_cros()
                          : sync_proto.user_display_mode_non_cros();
#else
  // Defaults to UNSPECIFIED, which will be converted to kStandalone.
  user_display_mode = sync_proto.user_display_mode_non_cros();
#endif  // BUILDFLAG(IS_CHROMEOS)
  return CreateUserDisplayModeFromWebAppSpecificsUserDisplayMode(
      user_display_mode);
}

}  // namespace web_app
