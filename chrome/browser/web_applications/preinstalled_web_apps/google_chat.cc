// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/google_chat.h"

#include "ash/constants/web_app_id_constants.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

namespace web_app {

ExternalInstallOptions GetConfigForGoogleChat(bool is_standalone,
                                              bool only_for_new_users) {
  constexpr std::string_view kOldInstallUrl =
      "https://mail.google.com/chat/download?usp=chrome_default";
  constexpr std::string_view kOldStartUrl = "https://mail.google.com/chat/";
  constexpr std::string_view kOldScope = "https://mail.google.com/chat/";
  constexpr std::string_view kOldManifestId = "https://mail.google.com/chat/";

  constexpr std::string_view kInstallUrl =
      "https://chat.google.com/download?usp=chrome_default";
  constexpr std::string_view kStartUrl = "https://chat.google.com/";
  constexpr std::string_view kScope = "https://chat.google.com/";
  constexpr std::string_view kManifestId = "https://chat.google.com/";

  const bool use_dedicated_origin_chat =
      base::FeatureList::IsEnabled(features::kWebAppMigratePreinstalledChat);

  ExternalInstallOptions options(
      /*install_url=*/
      GURL(use_dedicated_origin_chat ? kInstallUrl : kOldInstallUrl),
      /*user_display_mode=*/
      is_standalone ? mojom::UserDisplayMode::kStandalone
                    : mojom::UserDisplayMode::kBrowser,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  const webapps::ManifestId manifest_id = webapps::ManifestId(
      GURL(use_dedicated_origin_chat ? kManifestId : kOldManifestId));
  const GURL start_url =
      GURL(use_dedicated_origin_chat ? kStartUrl : kOldStartUrl);
  const GURL scope = GURL(use_dedicated_origin_chat ? kScope : kOldScope);

  // This allows the app to NOT be updated if the old version is installed by
  // any source other than this one (e.g. the user has also installed this app,
  // or it was policy installed.)
  if (use_dedicated_origin_chat) {
    options.SetOnlyUninstallAndReplaceWhenCompatible(
        GenerateAppIdFromManifestId(GURL(kOldManifestId)),
        ExternalInstallOptions::
            SetOnlyUninstallAndReplaceWhenCompatiblePassKey());
  }

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.only_for_new_users = only_for_new_users;
  options.expected_app_id = use_dedicated_origin_chat
                                ? ash::kGoogleChatAppId
                                : ash::kOldGoogleChatAppId;
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating(
      [](bool is_standalone, webapps::ManifestId manifest_id, GURL start_url,
         GURL scope) {
        auto info = std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
        // Note: Google Chat does not yet localize their app name, so nothing to
        // localize yet here.
        info->title = u"Google Chat";
        info->scope = scope;
        info->display_mode =
            is_standalone ? DisplayMode::kStandalone : DisplayMode::kBrowser;
        info->icon_bitmaps.any = LoadBundledIcons(
            {IDR_PREINSTALLED_WEB_APPS_GOOGLE_CHAT_ICON_192_PNG});
        return info;
      },
      is_standalone, manifest_id, start_url, scope);

  return options;
}

}  // namespace web_app
