// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_ui.h"

#include "chrome/grit/new_tab_footer_resources.h"
#include "chrome/grit/new_tab_footer_resources_map.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

bool NewTabFooterUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(ntp_features::kNtpFooter);
}

NewTabFooterUI::NewTabFooterUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://newtab-footer source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUINewTabFooterHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, kNewTabFooterResources,
                              IDR_NEW_TAB_FOOTER_NEW_TAB_FOOTER_HTML);

  // As a demonstration of passing a variable for JS to use we pass in some
  // a simple message.
  // TODO(crbug.com/409056431): Remove "Hello World!" once relevant strings are
  // added. This is used as a placeholder.
  source->AddString("message", "Hello World!");
}

NewTabFooterUI::~NewTabFooterUI() = default;
