// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_home/tab_group_home_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/grit/tab_group_home_resources.h"
#include "chrome/grit/tab_group_home_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace tabs {

TabGroupHomeUI::TabGroupHomeUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kTabGroupHomeHost);
  webui::SetupWebUIDataSource(source, kTabGroupHomeResources,
                              IDR_TAB_GROUP_HOME_TAB_GROUP_HOME_HTML);
}

TabGroupHomeUI::~TabGroupHomeUI() = default;

bool TabGroupHomeUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(kTabGroupHome);
}

WEB_UI_CONTROLLER_TYPE_IMPL(TabGroupHomeUI)

}  // namespace tabs
