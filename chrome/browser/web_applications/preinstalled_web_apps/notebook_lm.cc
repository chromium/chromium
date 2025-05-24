// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/notebook_lm.h"

#include <memory>

#include "ash/constants/web_app_id_constants.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "url/gurl.h"

namespace web_app {

ExternalInstallOptions GetConfigForNotebookLm() {
  static constexpr char kInstallUrl[] = "https://notebooklm.google.com/install";
  static constexpr char kStartUrl[] = "https://notebooklm.google.com/";
  static constexpr char kScopeUrl[] = "https://notebooklm.google.com/";
  static constexpr char kManifestId[] = "";

  ExternalInstallOptions options(
      /*install_url=*/GURL(kInstallUrl),
      /*user_display_mode=*/mojom::UserDisplayMode::kStandalone,
      /*install_source=*/ExternalInstallSource::kExternalDefault);
  options.user_type_allowlist = {apps::kUserTypeUnmanaged};
  options.add_to_applications_menu = true;
  options.add_to_search = true;
  options.expected_app_id = ash::kNotebookLmAppId;
  options.gate_on_feature = chromeos::features::kNotebookLmAppPreinstall.name;
  options.is_preferred_app_for_supported_links = true;

  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    webapps::ManifestId manifest_id =
        GenerateManifestId(kManifestId, GURL(kStartUrl));
    auto info =
        std::make_unique<WebAppInstallInfo>(manifest_id, GURL(kStartUrl));
    info->theme_color = SkColorSetARGB(0xFF, 0x00, 0x00, 0x00);
    info->background_color = SkColorSetARGB(0xFF, 0xFF, 0xFF, 0xFF);
    info->display_mode = blink::mojom::DisplayMode::kStandalone;
    info->icon_bitmaps.any =
        LoadBundledIcons({IDR_PREINSTALLED_WEB_APPS_NOTEBOOK_LM_ICON_512_PNG});
    info->scope = GURL(kScopeUrl);
    info->title = u"NotebookLM";
    return info;
  });

  return options;
}

}  // namespace web_app
