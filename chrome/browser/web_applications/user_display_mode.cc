// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/user_display_mode.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/features.h"
#include "components/sync/protocol/web_app_specifics.pb.h"

namespace web_app {

sync_pb::WebAppSpecifics::UserDisplayMode ToWebAppSpecificsUserDisplayMode(
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

mojom::UserDisplayMode ToMojomUserDisplayMode(
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

sync_pb::WebAppSpecifics::UserDisplayMode
ResolvePlatformSpecificUserDisplayMode(
    const sync_pb::WebAppSpecifics& sync_proto) {
  sync_pb::WebAppSpecifics_UserDisplayMode resolved_default_udm =
      sync_proto.has_user_display_mode_default()
          ? sync_proto.user_display_mode_default()
          : sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE;
#if BUILDFLAG(IS_CHROMEOS)
  return sync_proto.has_user_display_mode_cros()
             ? sync_proto.user_display_mode_cros()
             : resolved_default_udm;
#else
  return resolved_default_udm;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void SetPlatformSpecificUserDisplayMode(
    sync_pb::WebAppSpecifics::UserDisplayMode user_display_mode,
    sync_pb::WebAppSpecifics* sync_proto) {
#if BUILDFLAG(IS_CHROMEOS)
  sync_proto->set_user_display_mode_cros(user_display_mode);
#else
  sync_proto->set_user_display_mode_default(user_display_mode);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool HasCurrentPlatformUserDisplayMode(
    const sync_pb::WebAppSpecifics& sync_proto) {
#if BUILDFLAG(IS_CHROMEOS)
  return sync_proto.has_user_display_mode_cros();
#else
  return sync_proto.has_user_display_mode_default();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace web_app
