// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_internals_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/media_router/media_router_internals_webui_message_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace media_router {

MediaRouterInternalsUI::MediaRouterInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Create a WebUIDataSource containing the chrome://media-router-internals
  // page's content.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          Profile::FromWebUI(web_ui),
          chrome::kChromeUIMediaRouterInternalsHost);
  html_source->AddResourcePath("media_router_internals.js",
                               IDR_MEDIA_ROUTER_INTERNALS_JS);
  html_source->AddResourcePath("media_router_internals.css",
                               IDR_MEDIA_ROUTER_INTERNALS_CSS);
  html_source->SetDefaultResource(IDR_MEDIA_ROUTER_INTERNALS_HTML);

  content::WebContents* wc = web_ui->GetWebContents();
  DCHECK(wc);
  content::BrowserContext* context = wc->GetBrowserContext();
  MediaRouter* router = MediaRouterFactory::GetApiForBrowserContext(context);
  auto handler =
      std::make_unique<MediaRouterInternalsWebUIMessageHandler>(router);
  web_ui->AddMessageHandler(std::move(handler));
}

MediaRouterInternalsUI::~MediaRouterInternalsUI() = default;

}  // namespace media_router
