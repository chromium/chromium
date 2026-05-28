// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/personal_context_internals/personal_context_internals_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/personal_context_internals/personal_context_internals_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/personal_context_internals_resources.h"
#include "chrome/grit/personal_context_internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

PersonalContextInternalsUI::PersonalContextInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUIPersonalContextInternalsHost);

  webui::SetupWebUIDataSource(
      source, kPersonalContextInternalsResources,
      IDR_PERSONAL_CONTEXT_INTERNALS_PERSONAL_CONTEXT_INTERNALS_HTML);
}

PersonalContextInternalsUI::~PersonalContextInternalsUI() = default;

void PersonalContextInternalsUI::BindInterface(
    mojo::PendingReceiver<
        browser::personal_context_internals::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void PersonalContextInternalsUI::CreatePageHandler(
    mojo::PendingReceiver<
        browser::personal_context_internals::mojom::PageHandler> handler) {
  page_handler_ = std::make_unique<PersonalContextInternalsPageHandler>(
      std::move(handler), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents());
}

WEB_UI_CONTROLLER_TYPE_IMPL(PersonalContextInternalsUI)
