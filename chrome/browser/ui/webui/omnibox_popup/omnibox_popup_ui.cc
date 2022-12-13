// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/omnibox_popup_resources.h"
#include "chrome/grit/omnibox_popup_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"

OmniboxPopupUI::OmniboxPopupUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIOmniboxPopupHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kOmniboxPopupResources, kOmniboxPopupResourcesSize),
      IDR_OMNIBOX_POPUP_OMNIBOX_POPUP_HTML);
  webui::EnableTrustedTypesCSP(source);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

OmniboxPopupUI::~OmniboxPopupUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(OmniboxPopupUI)
