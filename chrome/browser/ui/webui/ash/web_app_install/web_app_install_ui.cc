// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/web_app_install/web_app_install_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/web_app_install_resources.h"
#include "chrome/grit/web_app_install_resources_map.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace ash::web_app_install {

WebAppInstallDialogUI::WebAppInstallDialogUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIWebAppInstallDialogHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"cancel", IDS_CANCEL},
      {"install", IDS_INSTALL},
      {"installing", IDS_OFFICE_INSTALL_PWA_INSTALLING_BUTTON},
  };

  source->AddLocalizedStrings(kStrings);
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kWebAppInstallResources, kWebAppInstallResourcesSize),
      IDR_WEB_APP_INSTALL_MAIN_HTML);
}

WebAppInstallDialogUI::~WebAppInstallDialogUI() = default;

bool WebAppInstallDialogUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      chromeos::features::kCrosWebAppInstallDialog);
}

WebAppInstallDialogUIConfig::WebAppInstallDialogUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIWebAppInstallDialogHost) {}

}  // namespace ash::web_app_install
