// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tabstrip_ntb/tabstrip_ntb_ui.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/tabstrip_ntb_resources.h"
#include "chrome/grit/tabstrip_ntb_resources_map.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace tabstrip_ntb {

bool TabStripNtbUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      chrome::android::kAndroidNewTabButtonTabstripWebUI);
}

TabStripNtbUI::TabStripNtbUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUITabStripNtbHost);
  webui::SetupWebUIDataSource(source, kTabstripNtbResources,
                              IDR_TABSTRIP_NTB_TABSTRIP_NTB_HTML);

  source->AddLocalizedString("newTabButtonAccName", IDS_ACCNAME_NEWTAB);
}

TabStripNtbUI::~TabStripNtbUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(TabStripNtbUI)

}  // namespace tabstrip_ntb
