// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_USER_DISPLAY_MODE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_USER_DISPLAY_MODE_H_

#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "components/sync/protocol/web_app_specifics.pb.h"

namespace web_app {

sync_pb::WebAppSpecifics::UserDisplayMode ToWebAppSpecificsUserDisplayMode(
    mojom::UserDisplayMode user_display_mode);

mojom::UserDisplayMode ToMojomUserDisplayMode(
    sync_pb::WebAppSpecifics::UserDisplayMode display_mode);

// Returns the platform-specific UserDisplayMode field in `sync_proto`.
// If the current-platform's field is unset, falls back to the default field. If
// the default field is unset, falls back to Standalone.
sync_pb::WebAppSpecifics::UserDisplayMode
ResolvePlatformSpecificUserDisplayMode(
    const sync_pb::WebAppSpecifics& sync_proto);

// Set `user_display_mode` onto the platform-specific UserDisplayMode field in
// the `sync_proto` based on the current platform (CrOS or default).
void SetPlatformSpecificUserDisplayMode(
    sync_pb::WebAppSpecifics::UserDisplayMode user_display_mode,
    sync_pb::WebAppSpecifics* sync_proto);

// Return whether `sync_proto` has a UserDisplayMode set for the current
// platform. May be false for not-yet-migrated apps loaded from the database, or
// incoming sync data from another platform.
bool HasCurrentPlatformUserDisplayMode(
    const sync_pb::WebAppSpecifics& sync_proto);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_USER_DISPLAY_MODE_H_
