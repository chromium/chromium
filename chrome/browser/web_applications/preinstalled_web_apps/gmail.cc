// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/gmail.h"

#include "ash/constants/web_app_id_constants.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"

namespace web_app {

ExternalInstallOptions GetConfigForGmail() {
  ExternalInstallOptions options(
      /*install_url=*/GURL(
          "https://mail.google.com/mail/installwebapp?usp=chrome_default"),
#if BUILDFLAG(IS_CHROMEOS)
      /*user_display_mode=*/mojom::UserDisplayMode::kStandalone,
#else
      /*user_display_mode=*/mojom::UserDisplayMode::kBrowser,
#endif  // BUILDFLAG(IS_CHROMEOS)
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.uninstall_and_replace.push_back("pjkljhegncpnkpknbcohdijeoejaedia");
  options.disable_if_tablet_form_factor = true;
  options.load_and_await_service_worker_registration = false;
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    GURL start_url = GURL("https://mail.google.com/mail/?usp=installed_webapp");
    // `manifest_id` must remain fixed even if start_url changes.
    webapps::ManifestId manifest_id = GenerateManifestIdFromStartUrlOnly(
        GURL("https://mail.google.com/mail/?usp=installed_webapp"));
    auto info = std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
    info->title = u"Gmail";
    info->scope = GURL("https://mail.google.com/mail/");
    info->display_mode = DisplayMode::kBrowser;
    info->icon_bitmaps.any =
        LoadBundledIcons({IDR_PREINSTALLED_WEB_APPS_GMAIL_ICON_192_PNG});
    return info;
  });
  options.expected_app_id = kGmailAppId;

  return options;
}

}  // namespace web_app
