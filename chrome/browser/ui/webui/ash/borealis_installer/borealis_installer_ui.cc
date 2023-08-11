// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer_ui.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/borealis_installer_resources.h"
#include "chrome/grit/borealis_installer_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

bool BorealisInstallerUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kBorealisWebUIInstaller);
}

BorealisInstallerUI::BorealisInstallerUI(content::WebUI* web_ui)
    : ui::MojoWebUIController{web_ui} {
  // Set up the chrome://borealis-installer source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUIBorealisInstallerHost);

  webui::SetupWebUIDataSource(html_source,
                              base::make_span(kBorealisInstallerResources,
                                              kBorealisInstallerResourcesSize),
                              IDR_BOREALIS_INSTALLER_BOREALIS_INSTALLER_HTML);
}

BorealisInstallerUI::~BorealisInstallerUI() = default;

}  // namespace ash
