// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_USER_DISPLAY_MODE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_USER_DISPLAY_MODE_H_

#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "components/sync/protocol/web_app_specifics.pb.h"

namespace web_app {

::sync_pb::WebAppSpecifics::UserDisplayMode
ConvertUserDisplayModeToWebAppSpecificsUserDisplayMode(
    mojom::UserDisplayMode user_display_mode);

mojom::UserDisplayMode CreateUserDisplayModeFromWebAppSpecificsUserDisplayMode(
    ::sync_pb::WebAppSpecifics::UserDisplayMode display_mode);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_USER_DISPLAY_MODE_H_
