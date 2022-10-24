// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/lens/lens_untrusted_ui.h"

#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/lens_untrusted_resources.h"
#include "chrome/grit/lens_untrusted_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace lens {

LensUntrustedUI::LensUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  // Set up the chrome-untrusted://lens source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUILensUntrustedURL);
  // Add required resources.
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kLensUntrustedResources, kLensUntrustedResourcesSize),
      IDR_LENS_UNTRUSTED_REGION_SEARCH_UNTRUSTED_HTML);
  html_source->AddFrameAncestor(GURL(chrome::kChromeUILensURL));
  // Allows chrome:://lens to load this page in an iframe.
  html_source->OverrideCrossOriginOpenerPolicy("same-origin");
  html_source->OverrideCrossOriginEmbedderPolicy("require-corp");
}

}  // namespace lens
