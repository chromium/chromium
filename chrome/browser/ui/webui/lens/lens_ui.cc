// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/lens/lens_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/lens_resources.h"
#include "chrome/grit/lens_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

LensUI::LensUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://lens source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(Profile::FromWebUI(web_ui),
                                             chrome::kChromeUILensHost);
  webui::SetupWebUIDataSource(
      html_source, base::make_span(kLensResources, kLensResourcesSize),
      IDR_LENS_REGION_SEARCH_HTML);

  // Set up Content Security Policy (CSP) for chrome-untrusted://lens iframe.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
  // Allow chrome-untrusted://lens page to load as an iframe in the page.
  std::string frame_src = base::StringPrintf(
      "frame-src %s 'self';", chrome::kChromeUILensOverlayUntrustedURL);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, frame_src);
}

LensUI::~LensUI() = default;
