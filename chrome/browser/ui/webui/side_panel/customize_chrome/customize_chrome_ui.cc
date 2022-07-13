// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/side_panel_customize_chrome_resources.h"
#include "chrome/grit/side_panel_customize_chrome_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

CustomizeChromeUI::CustomizeChromeUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUICustomizeChromeSidePanelHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kSidePanelCustomizeChromeResources,
                      kSidePanelCustomizeChromeResourcesSize),
      IDR_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_HTML);

  content::WebUIDataSource::Add(profile, source);
}

CustomizeChromeUI::~CustomizeChromeUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(CustomizeChromeUI)
