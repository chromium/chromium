// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/google_chat.h"

#include "ash/constants/web_app_id_constants.h"
#include "base/functional/bind.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

ExternalInstallOptions GetConfigForGoogleChat(bool is_standalone,
                                              bool only_for_new_users) {
  ExternalInstallOptions options(
      /*install_url=*/GURL(
          "https://mail.google.com/chat/download?usp=chrome_default"),
      /*user_display_mode=*/
      is_standalone ? mojom::UserDisplayMode::kStandalone
                    : mojom::UserDisplayMode::kBrowser,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  // Exclude managed users until we have a way for admins to block the app.
  options.user_type_allowlist = {"unmanaged"};
  options.only_for_new_users = only_for_new_users;
  options.expected_app_id = ash::kGoogleChatAppId;

// Specifying the factory seems to interfere with "APS" migration on CrOS.
#if !BUILDFLAG(IS_CHROMEOS)
  options.app_info_factory = base::BindRepeating(
      [](bool is_standalone) {
        GURL start_url = GURL("https://mail.google.com/chat/");
        // `manifest_id` must remain fixed even if start_url changes.
        webapps::ManifestId manifest_id = GenerateManifestIdFromStartUrlOnly(
            GURL("https://mail.google.com/chat/"));
        auto info = std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
        // Note: Google Chat does not yet localize their app name, so nothing to
        // localize yet here.
        info->title = u"Google Chat";
        info->scope = GURL("https://mail.google.com/chat/");
        info->display_mode =
            is_standalone ? DisplayMode::kStandalone : DisplayMode::kBrowser;
        info->icon_bitmaps.any = LoadBundledIcons(
            {IDR_PREINSTALLED_WEB_APPS_GOOGLE_CHAT_ICON_192_PNG});
        return info;
      },
      is_standalone);
#endif

  return options;
}

}  // namespace web_app
