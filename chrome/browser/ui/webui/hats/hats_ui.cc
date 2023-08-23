// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/hats/hats_ui.h"

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/hats_resources.h"
#include "chrome/grit/hats_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

HatsUIConfig::HatsUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  chrome::kChromeUIUntrustedHatsHost) {}

bool HatsUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kHaTSWebUI);
}

std::unique_ptr<content::WebUIController> HatsUIConfig::CreateWebUIController(
    content::WebUI* web_ui,
    const GURL& url) {
  return std::make_unique<HatsUI>(web_ui);
}

HatsUI::HatsUI(content::WebUI* web_ui) : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUntrustedHatsURL);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source, base::make_span(kHatsResources, kHatsResourcesSize),
      IDR_HATS_HATS_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(HatsUI)
