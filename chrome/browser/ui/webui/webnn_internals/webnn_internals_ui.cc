// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webnn_internals/webnn_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webnn_internals/webnn_internals_handler.h"
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
                              IDR_WEBNN_INTERNALS_INDEX_HTML);
}

WebNNInternalsUI::~WebNNInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(WebNNInternalsUI)

void WebNNInternalsUI::BindInterface(
    mojo::PendingReceiver<webnn_internals::mojom::WebNNInternalsHandlerFactory>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void WebNNInternalsUI::CreateWebNNInternalsHandler(
    mojo::PendingRemote<webnn_internals::mojom::WebNNInternalsPage> page,
    mojo::PendingReceiver<webnn_internals::mojom::WebNNInternalsHandler>
        handler) {
  handler_ = std::make_unique<WebNNInternalsHandler>(std::move(handler),
                                                     std::move(page));
}
