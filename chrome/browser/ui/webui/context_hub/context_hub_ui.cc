// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/context_hub/context_hub_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/context_hub_resources.h"
#include "chrome/grit/context_hub_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

ContextHubUI::ContextHubUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIContextHubHost);

  webui::SetupWebUIDataSource(source, kContextHubResources,
                              IDR_CONTEXT_HUB_CONTEXT_HUB_HTML);
}

ContextHubUI::~ContextHubUI() = default;

void ContextHubUI::BindInterface(
    mojo::PendingReceiver<browser::context_hub::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ContextHubUI::CreatePageHandler(
    mojo::PendingReceiver<browser::context_hub::mojom::PageHandler> handler) {
  page_handler_ = std::make_unique<ContextHubPageHandler>(
      std::move(handler), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents());
}

WEB_UI_CONTROLLER_TYPE_IMPL(ContextHubUI)
