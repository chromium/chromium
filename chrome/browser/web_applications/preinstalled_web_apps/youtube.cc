// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/youtube.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_utils.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"

namespace web_app {

ExternalInstallOptions GetConfigForYouTube() {
  ExternalInstallOptions options(
      /*install_url=*/GURL(
          "https://www.youtube.com/s/notifications/manifest/cr_install.html"),
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
      /*user_display_mode=*/DisplayMode::kStandalone,
#else
      /*user_display_mode=*/DisplayMode::kBrowser,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed"};
  options.gate_on_feature = kMigrateDefaultChromeAppToWebAppsNonGSuite.name;
  options.uninstall_and_replace.push_back("blpcfgokakmgnkcojhhkbfbldkacnbeo");
  options.disable_if_tablet_form_factor = true;
  options.load_and_await_service_worker_registration = false;
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->title = u"YouTube";
    info->start_url = GURL("https://www.youtube.com/?feature=ytca");
    info->scope = GURL("https://www.youtube.com/");
    info->display_mode = DisplayMode::kMinimalUi;
    info->icon_bitmaps.any =
        LoadBundledIcons({IDR_PREINSTALLED_WEB_APPS_YOUTUBE_ICON_192_PNG});
    return info;
  });

  return options;
}

}  // namespace web_app
