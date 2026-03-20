// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webnn_internals/webnn_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webnn_internals/webnn_internals_page_handler_impl.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/webnn_internals_resources.h"
#include "chrome/grit/webnn_internals_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

WebNNInternalsUI::WebNNInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIWebNNInternalsHost);

  webui::SetupWebUIDataSource(source, kWebnnInternalsResources,
                              IDR_WEBNN_INTERNALS_WEBNN_INTERNALS_HTML);
}

WebNNInternalsUI::~WebNNInternalsUI() = default;

void WebNNInternalsUI::BindInterface(
    mojo::PendingReceiver<webnn_internals::mojom::PageHandlerFactory>
        receiver) {
  webnn_internals_page_factory_receiver_.reset();
  webnn_internals_page_factory_receiver_.Bind(std::move(receiver));
}

void WebNNInternalsUI::CreatePageHandler(
    mojo::PendingRemote<webnn_internals::mojom::Page> page,
    mojo::PendingReceiver<webnn_internals::mojom::PageHandler> handler) {
  webnn_internals_page_handler_ =
      std::make_unique<WebNNInternalsPageHandlerImpl>(std::move(handler),
                                                      std::move(page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(WebNNInternalsUI)
