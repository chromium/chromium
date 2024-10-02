// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/media_router/media_router_internals_ui.h"

#include <memory>

#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/media_router/media_router_internals_webui_message_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/media_router_internals_resources.h"
#include "chrome/grit/media_router_internals_resources_map.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace media_router {

bool MediaRouterInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return MediaRouterEnabled(profile);
}

MediaRouterInternalsUI::MediaRouterInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Create a WebUIDataSource containing the chrome://media-router-internals
  // page's content.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          Profile::FromWebUI(web_ui),
          chrome::kChromeUIMediaRouterInternalsHost);

  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kMediaRouterInternalsResources,
                      kMediaRouterInternalsResourcesSize),
      IDR_MEDIA_ROUTER_INTERNALS_MEDIA_ROUTER_INTERNALS_HTML);

  content::WebContents* wc = web_ui->GetWebContents();
  DCHECK(wc);
  content::BrowserContext* context = wc->GetBrowserContext();
  MediaRouter* router = MediaRouterFactory::GetApiForBrowserContext(context);
  auto handler = std::make_unique<MediaRouterInternalsWebUIMessageHandler>(
      router, router->GetDebugger());
  web_ui->AddMessageHandler(std::move(handler));
}

MediaRouterInternalsUI::~MediaRouterInternalsUI() = default;

}  // namespace media_router
