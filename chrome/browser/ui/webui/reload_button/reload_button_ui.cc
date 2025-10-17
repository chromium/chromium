// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/reload_button/reload_button_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/reload_button_resources.h"
#include "chrome/grit/reload_button_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

ReloadButtonUI::ReloadButtonUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIReloadButtonHost);

  webui::SetupWebUIDataSource(source, kReloadButtonResources,
                              IDR_RELOAD_BUTTON_RELOAD_BUTTON_HTML);
}

ReloadButtonUI::~ReloadButtonUI() = default;

bool ReloadButtonUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::IsWebUIReloadButtonEnabled();
}
