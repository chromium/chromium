// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/google_meet.h"

namespace web_app {

ExternalInstallOptions GetConfigForGoogleMeet() {
  ExternalInstallOptions options(
      /*install_url=*/GURL(
          "https://meet.google.com/download/webapp?usp=chrome_default"),
      /*user_display_mode=*/DisplayMode::kStandalone,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  // Exclude managed users until we have a way for admins to block the app.
  options.user_type_allowlist = {"unmanaged", "child"};
  options.only_for_new_users = true;

  return options;
}

}  // namespace web_app
