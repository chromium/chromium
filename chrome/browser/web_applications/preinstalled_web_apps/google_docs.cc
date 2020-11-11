// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/google_docs.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_utils.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"

namespace web_app {

ExternalInstallOptions GetConfigForGoogleDocs() {
  ExternalInstallOptions options(
      /*install_url=*/GURL(
          "https://docs.google.com/document/installwebapp?usp=chrome_default"),
      /*user_display_mode=*/DisplayMode::kBrowser,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.gate_on_feature = "MigrateDefaultChromeAppToWebAppsGSuite";
  options.uninstall_and_replace.push_back("aohghmighlieiainnegkcijnfilokake");
  options.load_and_await_service_worker_registration = false;

#if !defined(OS_CHROMEOS)
  options.only_use_app_info_factory = true;
#endif  // !defined(OS_CHROMEOS)

  options.app_info_factory = base::BindRepeating([]() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->title = base::UTF8ToUTF16("Docs");
    info->start_url =
        GURL("https://docs.google.com/document/?usp=installed_webapp");
    info->scope = GURL("https://docs.google.com/document/");
    info->display_mode = DisplayMode::kBrowser;
    info->icon_bitmaps_any =
        LoadBundledIcons({IDR_PREINSTALLED_WEB_APPS_GOOGLE_DOCS_ICON_192_PNG});
    return info;
  });

  return options;
}

}  // namespace web_app
