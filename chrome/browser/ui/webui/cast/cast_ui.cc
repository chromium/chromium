// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cast/cast_ui.h"

#include "chrome/browser/media/router/event_page_request_manager.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

// TODO: Figure out a way to self-navigate the WebUI in C++ on instantiation,
// and replace the navigation in cast.js with that.
CastUI::CastUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Retrieve the ID of the component extension.
  auto* event_page_request_manager =
      media_router::EventPageRequestManagerFactory::GetApiForBrowserContext(
          web_ui->GetWebContents()->GetBrowserContext());
  std::string extension_id =
      event_page_request_manager->media_route_provider_extension_id();

  // Set up the chrome://cast data source and add required resources.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUICastHost);

  html_source->AddResourcePath("cast.js", IDR_CAST_JS);
  html_source->AddString("extensionId", extension_id);
  html_source->UseStringsJs();
  html_source->SetDefaultResource(IDR_CAST_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);
}

CastUI::~CastUI() {
}
