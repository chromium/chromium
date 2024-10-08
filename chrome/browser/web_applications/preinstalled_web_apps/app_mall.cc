// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/app_mall.h"

#include "ash/constants/web_app_id_constants.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/url_constants.h"
#include "url/gurl.h"

namespace web_app {

ExternalInstallOptions GetConfigForAppMall() {
  ExternalInstallOptions options(
      /*install_url=*/GURL(chromeos::kAppMallBaseUrl).Resolve("install/"),
      /*user_display_mode=*/mojom::UserDisplayMode::kStandalone,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged"};
  options.gate_on_feature = chromeos::features::kCrosMall.name;

  options.load_and_await_service_worker_registration = false;
  options.expected_app_id = kMallAppId;

  return options;
}

}  // namespace web_app
