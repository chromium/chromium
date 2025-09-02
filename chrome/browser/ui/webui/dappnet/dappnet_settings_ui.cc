// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/dappnet/dappnet_settings_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "chrome/grit/dappnet_settings_resources.h"
#include "chrome/grit/dappnet_settings_resources_map.h"

DappnetSettingsUIConfig::DappnetSettingsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIDappnetHost) {}

bool DappnetSettingsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return true;
}

DappnetSettingsUI::DappnetSettingsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(
          Profile::FromWebUI(web_ui), chrome::kChromeUIDappnetHost);

  // Back to using actual resources - the resource IDs should work
  source->AddResourcePath("", IDR_DAPPNET_SETTINGS_HTML);
  source->AddResourcePath("config", IDR_DAPPNET_SETTINGS_HTML);
  source->AddResourcePath("dappnet_settings.js", IDR_DAPPNET_SETTINGS_JS);
  source->AddResourcePath("dappnet_settings.css", IDR_DAPPNET_SETTINGS_CSS);
  source->AddResourcePath("dappnet_settings_api.js", IDR_DAPPNET_SETTINGS_API_JS);

  // Set default resource
  source->SetDefaultResource(IDR_DAPPNET_SETTINGS_HTML);
}

DappnetSettingsUI::~DappnetSettingsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(DappnetSettingsUI)