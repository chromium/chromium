// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/federated_learning/floc_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/federated_learning/floc_internals.mojom.h"
#include "chrome/browser/ui/webui/federated_learning/floc_internals_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/floc_internals_resources.h"
#include "chrome/grit/floc_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

FlocInternalsUI::FlocInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIFlocInternalsHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kFlocInternalsResources, kFlocInternalsResourcesSize),
      IDR_FLOC_INTERNALS_FLOC_INTERNALS_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

FlocInternalsUI::~FlocInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(FlocInternalsUI)

void FlocInternalsUI::BindInterface(
    mojo::PendingReceiver<federated_learning::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<FlocInternalsPageHandler>(
      Profile::FromBrowserContext(
          web_ui()->GetWebContents()->GetBrowserContext()),
      std::move(receiver));
}
