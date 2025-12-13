// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/vids.h"

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
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/part.h"
#include "third_party/liburlpattern/pattern.h"

namespace web_app {

namespace {

blink::Manifest::HomeTabParams HomeTabPathnames(
    std::vector<std::string_view> pathnames) {
  blink::Manifest::HomeTabParams home_tab;

  for (const std::string_view pathname : pathnames) {
    base::expected<liburlpattern::Pattern, absl::Status> parse_result =
        liburlpattern::Parse(pathname, [](std::string_view input) {
          return std::string(input);
        });
    CHECK(parse_result.has_value());

    blink::SafeUrlPattern url_pattern;
    url_pattern.pathname = std::move(parse_result.value().PartList());
    for (const liburlpattern::Part& part : url_pattern.pathname) {
      CHECK_NE(part.type, liburlpattern::PartType::kRegex);
    }
    home_tab.scope_patterns.push_back(std::move(url_pattern));
  }

  return home_tab;
}

}  // namespace

ExternalInstallOptions GetConfigForVids(bool is_standalone_tabbed) {
  ExternalInstallOptions options(
      /*install_url=*/GURL(
          "https://docs.google.com/videos/installwebapp?usp=chrome_default"),
      /*user_display_mode=*/
      is_standalone_tabbed ? mojom::UserDisplayMode::kStandalone
                           : mojom::UserDisplayMode::kBrowser,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"managed"};
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    GURL start_url =
        GURL("https://docs.google.com/videos/?usp=installed_webapp");
    webapps::ManifestId manifest_id =
        GenerateManifestId("videos/?usp=installed_webapp", start_url);
    auto info = std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
    info->title = u"Vids";
    info->scope = GURL("https://docs.google.com/videos/");
    info->display_mode = DisplayMode::kBrowser;
    info->display_override = {DisplayMode::kTabbed};
    info->tab_strip.emplace();
    info->tab_strip->new_tab_button.url =
        GURL("https://docs.google.com/videos/u/0/create?usp=webapp_tab_strip");
    info->tab_strip->home_tab = HomeTabPathnames({
        "/videos/", "/videos/u/:index", "/videos/u/:index/",
        // The manifest officially includes the following pathnames however they
        // are not in scope and would be ignored by blink::ManifestParser.
        // "/a/:domain/videos",
        // "/a/:domain/videos/",
    });
    info->icon_bitmaps.any =
        LoadBundledIcons({IDR_PREINSTALLED_WEB_APPS_VIDS_ICON_192_PNG});
    return info;
  });
  options.expected_app_id = ash::kVidsAppId;

  return options;
}

}  // namespace web_app
