// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/lens/lens_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/lens_resources.h"
#include "chrome/grit/lens_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

LensUI::LensUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://lens source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(Profile::FromWebUI(web_ui),
                                             chrome::kChromeUILensHost);
  webui::SetupWebUIDataSource(
      html_source, base::make_span(kLensResources, kLensResourcesSize),
      IDR_LENS_LENS_HTML);
}

LensUI::~LensUI() = default;
