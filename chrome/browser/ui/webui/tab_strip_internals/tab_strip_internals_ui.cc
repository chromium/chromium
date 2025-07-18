// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_ui.h"

#include "chrome/browser/ui/tabs/features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/tab_strip_internals_resources.h"
#include "chrome/grit/tab_strip_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

TabStripInternalsUI::TabStripInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Setup up the chrome://tab-strip-internals source
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUITabStripInternalsHost);

  // Add required resources
  webui::SetupWebUIDataSource(source, kTabStripInternalsResources,
                              IDR_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_HTML);
}

TabStripInternalsUI::~TabStripInternalsUI() = default;

bool TabStripInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return DefaultInternalWebUIConfig::IsWebUIEnabled(browser_context) &&
         base::FeatureList::IsEnabled(tabs::kDebugUITabStrip);
}
