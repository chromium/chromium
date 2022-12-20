// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feed/feed_ui.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/feed_resources.h"
#include "chrome/grit/feed_resources_map.h"
#include "components/feed/feed_feature_list.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/resource_path.h"

namespace feed {

FeedUI::FeedUI(content::WebUI* web_ui)
    : ui::UntrustedBubbleWebUIController(web_ui) {
  web_ui->AddRequestableScheme("https");
  // TODO(crbug.com/1292623): We should disable http requests before launching.
  web_ui->AddRequestableScheme("http");

  // Create a URLDataSource and add resources.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUntrustedFeedURL);
  webui::SetupWebUIDataSource(
      source, base::make_span(kFeedResources, kFeedResourcesSize),
      IDR_FEED_FEED_HTML);

  if (kWebUiDisableContentSecurityPolicy.Get()) {
    source->DisableContentSecurityPolicy();
  } else {
    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::FrameSrc,
        "frame-src https://www.google.com;");
    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::StyleSrc,
        "style-src 'unsafe-inline' 'self';");
  }

  // Configurable javascript for prototyping purposes.
  source->AddString("feedUrl", kWebUiFeedUrl.Get());
}

FeedUI::~FeedUI() = default;

void FeedUI::BindInterface(
    mojo::PendingReceiver<feed::mojom::FeedSidePanelHandlerFactory> factory) {
  if (side_panel_handler_factory_.is_bound())
    side_panel_handler_factory_.reset();
  side_panel_handler_factory_.Bind(std::move(factory));
}

void FeedUI::CreateFeedSidePanelHandler(
    mojo::PendingReceiver<feed::mojom::FeedSidePanelHandler> handler,
    mojo::PendingRemote<feed::mojom::FeedSidePanel> side_panel) {
  DCHECK(side_panel.is_valid());
  side_panel_handler_ =
      std::make_unique<FeedHandler>(std::move(handler), std::move(side_panel));
}

WEB_UI_CONTROLLER_TYPE_IMPL(FeedUI)

}  // namespace feed
