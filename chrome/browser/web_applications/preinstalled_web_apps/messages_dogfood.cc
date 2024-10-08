// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/messages_dogfood.h"

#include "ash/constants/web_app_id_constants.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"

namespace web_app {

ExternalInstallOptions GetConfigForMessagesDogfood() {
  ExternalInstallOptions options(
      /*install_url=*/GURL("https://messages.google.com/web/authentication"),
      /*user_display_mode=*/mojom::UserDisplayMode::kStandalone,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.uninstall_and_replace = {kMessagesDogfoodDeprecatedAppId};
  options.override_previous_user_uninstall = true;
  options.user_type_allowlist = {"managed"};

  return options;
}

}  // namespace web_app
