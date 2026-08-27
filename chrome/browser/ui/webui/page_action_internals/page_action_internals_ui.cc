// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/page_action_internals/page_action_internals_ui.h"

#include <memory>

#include "chrome/browser/ui/webui/page_action_internals/page_action_internals_handler.h"
#include "chrome/grit/page_action_internals_resources.h"
#include "chrome/grit/page_action_internals_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

PageActionInternalsUIConfig::PageActionInternalsUIConfig()
    : DefaultInternalWebUIConfig(chrome::kChromeUIPageActionInternalsHost) {}

PageActionInternalsUI::PageActionInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIPageActionInternalsHost);

  webui::SetupWebUIDataSource(
      source, kPageActionInternalsResources,
      IDR_PAGE_ACTION_INTERNALS_PAGE_ACTION_INTERNALS_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(PageActionInternalsUI)

PageActionInternalsUI::~PageActionInternalsUI() = default;

void PageActionInternalsUI::BindInterface(
    mojo::PendingReceiver<page_action_internals::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void PageActionInternalsUI::CreatePageHandler(
    mojo::PendingReceiver<page_action_internals::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<PageActionInternalsHandler>(std::move(receiver));
}
