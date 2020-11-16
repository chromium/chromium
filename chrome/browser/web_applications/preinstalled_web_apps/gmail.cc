// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/gmail.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_utils.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"

namespace web_app {

ExternalInstallOptions GetConfigForGmail() {
  ExternalInstallOptions options(
      /*install_url=*/GURL(
          "https://mail.google.com/mail/installwebapp?usp=chrome_default"),
      /*user_display_mode=*/DisplayMode::kBrowser,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.gate_on_feature = kMigrateDefaultChromeAppToWebAppsGSuite.name;
  options.uninstall_and_replace.push_back("pjkljhegncpnkpknbcohdijeoejaedia");
  options.load_and_await_service_worker_registration = false;
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->title = base::UTF8ToUTF16("Gmail");
    info->start_url = GURL("https://mail.google.com/?usp=installed_webapp");
    info->scope = GURL("https://mail.google.com/");
    info->display_mode = DisplayMode::kBrowser;
    info->icon_bitmaps_any =
        LoadBundledIcons({IDR_PREINSTALLED_WEB_APPS_GMAIL_ICON_192_PNG});
    return info;
  });

  return options;
}

}  // namespace web_app
