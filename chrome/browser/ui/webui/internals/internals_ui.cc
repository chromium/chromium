// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/internals_ui.h"

#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/webui/internals/notifications/notifications_internals_ui_message_handler.h"
#include "chrome/browser/ui/webui/internals/query_tiles/query_tiles_internals_ui_message_handler.h"
#else
#include "chrome/browser/ui/webui/internals/web_app/web_app_internals_page_handler_impl.h"
#include "chrome/grit/internals_resources.h"
#include "chrome/grit/internals_resources_map.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#endif  // defined(OS_ANDROID)

InternalsUI::InternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  profile_ = Profile::FromWebUI(web_ui);
  source_ = content::WebUIDataSource::Create(chrome::kChromeUIInternalsHost);

  // Add your sub-URL internals WebUI here.
  // Keep this set of sub-URLs in sync with |kChromeInternalsPathURLs|.
#if defined(OS_ANDROID)
  // chrome://internals/notifications
  AddNotificationsInternals(web_ui);
  // chrome://internals/query-tiles
  if (!profile_->IsOffTheRecord())
    AddQueryTilesInternals(web_ui);
#else
  source_->AddResourcePaths(
      base::make_span(kInternalsResources, kInternalsResourcesSize));
  source_->AddResourcePath("hello-ts", IDR_HELLO_TS_HELLO_TS_HTML);

  // chrome://internals/web-app
  WebAppInternalsPageHandlerImpl::AddPageResources(source_);
#endif  // defined(OS_ANDROID)

  content::WebUIDataSource::Add(profile_, source_);
}

InternalsUI::~InternalsUI() = default;

#if defined(OS_ANDROID)
void InternalsUI::AddNotificationsInternals(content::WebUI* web_ui) {
  source_->AddResourcePath("notifications_internals.js",
                           IDR_NOTIFICATIONS_INTERNALS_JS);
  source_->AddResourcePath("notifications_internals_browser_proxy.js",
                           IDR_NOTIFICATIONS_INTERNALS_BROWSER_PROXY_JS);
  source_->AddResourcePath("notifications", IDR_NOTIFICATIONS_INTERNALS_HTML);

  web_ui->AddMessageHandler(
      std::make_unique<NotificationsInternalsUIMessageHandler>(profile_));
}

void InternalsUI::AddQueryTilesInternals(content::WebUI* web_ui) {
  source_->AddResourcePath("query_tiles_internals.js",
                           IDR_QUERY_TILES_INTERNALS_JS);
  source_->AddResourcePath("query_tiles_internals_browser_proxy.js",
                           IDR_QUERY_TILES_INTERNALS_BROWSER_PROXY_JS);
  source_->AddResourcePath("query-tiles", IDR_QUERY_TILES_INTERNALS_HTML);
  web_ui->AddMessageHandler(
      std::make_unique<QueryTilesInternalsUIMessageHandler>(profile_));
}
#else   // defined(OS_ANDROID)
void InternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::web_app_internals::WebAppInternalsPageHandler>
        receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebAppInternalsPageHandlerImpl>(profile_),
      std::move(receiver));
}
#endif  // defined(OS_ANDROID)

WEB_UI_CONTROLLER_TYPE_IMPL(InternalsUI)
