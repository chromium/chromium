// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/app_mall.h"

#include <memory>

#include "ash/webui/grit/ash_help_app_resources.h"
#include "base/functional/bind.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"
#include "chromeos/constants/chromeos_features.h"

namespace web_app {

ExternalInstallOptions GetConfigForAppMall() {
  ExternalInstallOptions options(
      /*install_url=*/GURL("https://discover.apps.chrome/"),
      /*user_display_mode=*/mojom::UserDisplayMode::kStandalone,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged"};
  options.gate_on_feature = chromeos::features::kCrosMall.name;

  options.load_and_await_service_worker_registration = false;
  options.only_use_app_info_factory = false;

  // This App Info Factory is temporary, to help with prototyping.
  // TODO(b/327080071): Remove.
  options.app_info_factory = base::BindRepeating([]() {
    auto info = std::make_unique<WebAppInstallInfo>();
    info->title = u"Get Apps and Games";
    info->start_url = GURL("https://discover.apps.chrome/");
    info->manifest_id = GURL("https://discover.apps.chrome/");
    info->display_mode = DisplayMode::kStandalone;
    info->icon_bitmaps.any = LoadBundledIcons({IDR_HELP_APP_ICON_192});
    return info;
  });

  return options;
}

}  // namespace web_app
